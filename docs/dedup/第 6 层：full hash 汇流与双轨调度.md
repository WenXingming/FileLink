# 第 6 层：full hash 汇流与双轨调度

## 优化背景

经过前面几层优化后，`dedup` 的候选文件已经被不断缩小：

```text
第 1 层 size 过滤：
  排除大小唯一、一定不可能重复的文件

第 2 层硬链接折叠：
  避免同一物理文件被重复读取和重复 hash

第 3 层小文件与大文件分流：
  将候选文件拆成 smallCandidates 和 largeCandidates

第 4 层小文件固定成本优化：
  降低小文件 full hash 时的 per-file overhead

第 5 层大文件 partial hash：
  用 head + tail partial hash 过滤掉明显不同的大文件
```

但是，这些优化都不是最终的重复判断。无论是 size、device/inode，还是 partial hash，它们本质上都只是过滤条件或执行优化。真正判断两个普通文件内容是否重复，最终仍然要依赖完整内容 hash。

因此，前面几层优化之后，流程必须重新汇流到 full hash 阶段：

```text
小文件轨道：
  smallCandidates
    -> full hash

大文件轨道：
  largeCandidates
    -> partial hash
    -> partialCollisions
    -> full hash

最终：
  full hash 结果
    -> find_exact_duplicates()
    -> 输出重复文件组
```

这一层的作用就是：把小文件轨道和大文件轨道重新汇流，并根据两类文件的成本差异，采用不同的 full hash 调度策略。

## 设计逻辑

full hash 阶段有两个目标。

第一个目标是保证正确性。前面的 partial hash 只能说明“两个文件仍然可能重复”，不能直接判断重复；hardlink follower 也只是暂时折叠了重复 I/O，最终仍然要展开回结果集。因此，最终输出重复文件组之前，必须有一层完整 hash 作为内容确认。

第二个目标是控制调度成本。小文件和大文件虽然最终都要进入 full hash，但它们不能使用完全相同的任务粒度。

对于小文件来说，单个文件读取量很小，任务本身很短。如果仍然保持：

```text
1 个文件 = 1 个任务 = 1 个 future
```

那么在大量小文件场景下，任务提交、队列同步和 future 管理成本会被放大。因此，小文件可以使用 batch，把多个小文件放到同一个任务中处理：

```text
小文件：
  一批文件 = 1 个任务 = 1 个 future
```

这样做的目标是减少任务数量和 future 数量，降低小文件的调度开销。

但大文件不能这样处理。

大文件单个任务本身就很重，如果一个 worker 一次拿到多个大文件，就会长时间占用这个 worker。其他线程可能很快完成任务并空闲，最终整体耗时被最慢的 batch 拖住。这就是典型的长尾问题。

因此，大文件进入 full hash 阶段时应该保持细粒度调度：

```text
大文件：
  1 个文件 = 1 个任务 = 1 个 future
```

也就是大文件的 `Batch Size = 1`。

所以这一层的核心设计可以概括为：

```text
小文件：
  batch 调度
  目标是减少调度和 future 管理成本

大文件：
  单文件调度
  目标是保持负载均衡，避免长尾任务
```

这就是 full hash 阶段的双轨调度策略。

## 实现流程

在代码上，前面的大文件 partial hash 会得到 `partialCollisions`，它表示 partial hash 之后仍然可能重复的大文件候选。

小文件则直接来自 `smallCandidates`。

full hash 阶段将这两部分重新汇流：

```cpp
std::vector<std::vector<FileInfo>> fullBatches =
    create_batches(smallCandidates, BATCH_SIZE);

const std::vector<std::vector<FileInfo>> largeBatches =
    create_batches(partialCollisions, 1);

fullBatches.insert(
    fullBatches.end(),
    largeBatches.begin(),
    largeBatches.end()
);

const std::vector<HashResult> fullHashes =
    compute_full_hashes(fullBatches);
```

这里可以分成两条轨道理解：

```text
轨道 A：小文件 full hash
  smallCandidates
    -> create_batches(smallCandidates, BATCH_SIZE)
    -> 多个小文件组成一个 batch
    -> 减少任务数量

轨道 B：大文件 full hash
  partialCollisions
    -> create_batches(partialCollisions, 1)
    -> 每个大文件单独成为一个 batch
    -> 保持线程池动态负载均衡
```

最后，两个轨道合并到同一个 `fullBatches` 中，再统一交给 `compute_full_hashes()` 执行。

这个设计的关键点是：汇流不代表使用同一种调度策略。小文件和大文件最终都进入 full hash，但它们进入 full hash 的任务粒度不同。

```text
汇流的是结果语义：
  最终都要计算 full hash

分开的调度策略：
  小文件 batch
  大文件 batch size = 1
```

这样既能保留小文件 batch 降低调度开销的可能性，又能避免大文件 batch 导致长尾。

full hash 完成后，还需要将前面硬链接折叠掉的 follower 展开回来：

```text
fullHashes
  -> 展开 hardlink follower
  -> unfoldedHashes
  -> find_exact_duplicates(unfoldedHashes)
```

因此，最终参与重复文件分组的仍然是所有路径，而不是只包含代表物理文件。

## Benchmark 结果与分析

这一层最重要的 benchmark 结论来自 V3 和 V4 的对比，但解释时必须非常谨慎。

V3 的初衷是减少小文件任务数量，因此引入 batch 策略。但是 V3 的问题在于：batch 策略被统一应用到了大文件。这样会导致一个 worker 可能一次拿到多个大文件，形成长尾任务，严重破坏负载均衡。

V4 修正了这个问题：

```text
小文件：
  仍然可以 batch

大文件：
  full hash 阶段使用 Batch Size = 1
```

从 benchmark 看，修复大文件调度问题后，大文件真实重复场景恢复到了合理水平：

| Workload           | V2      | V3      | V4      | 解释                                                    |
| ------------------ | ------- | ------- | ------- | ------------------------------------------------------- |
| `large_duplicates` | 77.4 ms | 516 ms  | 78.5 ms | V3 退化来自大文件 batch 长尾，V4 修复后基本回到 V2 水平 |
| `mixed_realistic`  | 461 ms  | 1505 ms | 358 ms  | V4 同时修复大文件长尾，并获得 partial hash 净收益       |

`large_duplicates` 是最能说明调度问题的 workload。因为这些大文件是真的重复，partial hash 不能把它们排除，最终必须 full hash。也就是说，这个场景并不会因为 partial hash 而少读数据。

因此，`large_duplicates` 从 V3 的 `516 ms` 降到 V4 的 `78.5 ms`，主要说明的是：V4 修复了 V3 的大文件 batch 长尾问题。而 V4 相比 V2 的 `77.4 ms -> 78.5 ms` 基本持平，也符合预期：真实重复大文件最终仍然必须完整读取和 hash。

这正好证明了双轨调度的必要性：

```text
如果大文件被错误地 batch：
  容易形成长尾任务，线程池负载不均衡

如果大文件使用 Batch Size = 1：
  worker 执行完一个大文件后可以继续取下一个任务
  线程池能够更好地动态分配工作量
```

对于 `mixed_realistic`，V4 从 V3 的 `1505 ms` 降到 `358 ms`，提升很明显。但这个提升包含两部分：

```text
第一部分：
  修复 V3 大文件 batch 长尾回归

第二部分：
  大文件 partial hash 过滤掉部分无效 full hash
```

因此，更严谨的净收益应该看 V2 到 V4：`mixed_realistic` 从 `461 ms` 降到 `358 ms`，约 `1.29x`。这说明在混合场景中，V4 不只是修复回归，也确实带来了额外收益。

对于非重复大文件，V4 的收益主要来自上一层 partial hash，而不是本层调度本身：

| Workload                                     | V2      | V4      | 解释                                                 |
| -------------------------------------------- | ------- | ------- | ---------------------------------------------------- |
| `large_same_size_different_content`          | 76.9 ms | 1.93 ms | partial hash 快速排除非重复大文件，约 39.8x          |
| `large_same_size_same_prefix_different_tail` | 77.1 ms | 1.97 ms | head + tail 过滤前缀相同但尾部不同的大文件，约 39.1x |

所以 V4 的整体收益要拆开看：

```text
partial hash：
  解决非重复大文件的无效 I/O

Batch Size = 1：
  解决大文件 full hash 阶段的长尾调度问题

full hash 汇流：
  保证最终重复判断仍然由完整内容 hash 完成
```

这一点非常适合在面试中强调：不是看到 V3 到 V4 提升很大，就简单归因于 partial hash；而是要区分“减少无效 I/O”和“修复任务调度长尾”这两个不同问题。

## 小结

第 6 层的核心是 full hash 汇流与双轨调度。

前面的 size 过滤、硬链接折叠、小文件分流和 partial hash 都是在减少候选、降低成本或优化执行方式，但最终重复判断仍然必须回到 full hash。小文件直接进入 full hash，大文件经过 partial hash 过滤后，只有 partial hash 仍然碰撞的候选才进入 full hash。

在 full hash 阶段，小文件和大文件采用不同的任务粒度。小文件可以 batch，以减少任务数量和 future 管理成本；大文件必须保持 `Batch Size = 1`，以避免一个 worker 拿到多个大文件造成长尾。

Benchmark 结果证明，这个修正非常必要。V3 将 batch 策略统一应用到大文件，导致 `large_duplicates` 从 V2 的 `77.4 ms` 退化到 `516 ms`；V4 将大文件 full hash 恢复为单文件调度后，结果回到 `78.5 ms`，基本与 V2 持平。对于混合场景，V4 相比 V2 从 `461 ms` 降到 `358 ms`，说明修复调度问题之外，partial hash 也带来了真实净收益。

因此，这一层的定位可以概括为：

```text
前面的漏斗负责减少进入 full hash 的候选；
full hash 汇流负责最终正确性；
双轨调度负责在小文件调度成本和大文件负载均衡之间做权衡。
```
