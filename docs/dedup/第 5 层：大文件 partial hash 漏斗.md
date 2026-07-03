# 第 5 层：大文件 partial hash 漏斗

## 优化背景

经过前面的几层优化之后，`dedup` 已经完成了候选文件筛选、硬链接折叠，以及小文件和大文件分流。

此时，大文件会进入 `largeCandidates` 轨道。大文件和小文件的成本模型完全不同。小文件的主要问题是每个文件都会发生的固定成本，例如 buffer 分配、任务提交、future 管理等；而大文件的主要问题是 I/O 成本。

对于一个大文件来说，full hash 意味着必须从头到尾读取完整文件内容：

```text
large file
  -> read entire file
  -> compute full hash
```

如果两个大文件内容确实相同，那么 full hash 是必要的，因为最终必须依赖完整内容 hash 来确认重复。但是在实际场景中，经常会出现另一种情况：多个大文件大小相同，但内容并不相同。例如：

```text
fileA: 8 MiB
fileB: 8 MiB
fileC: 8 MiB
```

它们会通过第 1 层 size 过滤，因为 size 相同；但 size 相同只是“可能重复”，并不代表内容真的相同。如果直接对这些文件全部计算 full hash，就会读取大量最终没有必要读取的数据。

因此，大文件优化的核心目标不是减少每个文件的固定成本，而是：

```text
尽量避免不必要的完整文件读取，
把昂贵的 full hash 留给真正需要验证的候选文件。
```

这就是大文件 partial hash 漏斗的出发点。

## 设计逻辑

大文件 partial hash 的基本思路是：在 full hash 之前，先读取文件的一小部分内容，计算一个低成本的局部 hash，用它快速排除明显不同的大文件。流程可以抽象为：

```text
largeCandidates
  -> 计算 partial hash
  -> 按 partial hash 分组
  -> partial hash 不同：直接排除，不进入 full hash
  -> partial hash 相同：保留为候选，继续 full hash
```

这一层仍然遵循整个 dedup 优化的漏斗思想：

```text
越便宜的信息越靠前使用；
越昂贵的操作越靠后执行。
```

对于大文件来说，读取完整文件非常昂贵；而读取头尾少量字节成本很低。因此，可以先用 partial hash 做一次低成本过滤。

需要注意，partial hash 不能作为最终重复判断依据。它只是 full hash 之前的候选筛选条件。最终两个文件是否重复，仍然必须由完整内容 hash 判断。也就是说：

```text
partial hash 不同：
  一定不是重复文件，可以跳过 full hash

partial hash 相同：
  只能说明“仍然可能重复”，不能直接判定重复
  还必须进入 full hash
```

这样既能保证正确性，又能减少无效 I/O。

## 为什么使用 head + tail

partial hash 有很多设计方式。最简单的方式是只读取文件头部，例如读取前 4KiB。但是只读头部并不够稳。

很多文件可能有相同的文件头，例如相同格式的媒体文件、压缩文件、日志文件、数据库文件，或者某些由同一程序生成的文件。它们的 header 或前缀可能非常相似，但尾部内容已经不同。如果只读取 head，就可能出现这种情况：

```text
fileA:
  head 相同
  middle 不同
  tail 不同

fileB:
  head 相同
  middle 不同
  tail 不同
```

只看 head 时，它们仍然会被认为是 partial hash 碰撞，无法提前过滤。

因此，V4 采用 head + tail 的局部哈希策略：

```text
读取文件头部一小段
读取文件尾部一小段
组合计算 partial hash
```

这样做的好处是：如果两个大文件只是前缀相同，但尾部不同，就可以在 partial hash 阶段被排除，不需要进入 full hash。

整体流程可以写成：

```text
size 相同的大文件
  -> 读取 head 2KiB + tail 2KiB
  -> 计算 partial hash
  -> partial hash 不同，跳过 full hash
  -> partial hash 相同，再进入 full hash
```

这一步的核心收益来自“少读”。对于 8MiB 的大文件，如果只读取头尾几 KiB 就能排除，那么相比完整读取整个文件，I/O 成本会大幅下降。

> [!note]
>
> 首、首 + 尾、首 + 中 + 尾的权衡。如果使用首+中+尾，可能增加磁盘访问的随机性性能可能下降（具体得实验做 benchmark）。

## 实现流程

在代码结构上，大文件轨道会先对 `largeCandidates` 创建 partial hash 任务：

```cpp
const size_t BATCH_SIZE = 64;

const std::vector<std::vector<FileInfo>> partialBatches =
    create_batches(largeCandidates, BATCH_SIZE);

const std::vector<HashResult> partialHashes =
    compute_partial_hashes(partialBatches);

const std::vector<FileInfo> partialCollisions =
    find_partial_collisions(partialHashes);
```

这里的含义是：

```text
largeCandidates:
  第 3 层分流后得到的大文件候选集合

partialBatches:
  将大文件候选切成若干 partial hash 任务

compute_partial_hashes:
  对每个大文件计算 head + tail partial hash

find_partial_collisions:
  找出 partial hash 仍然发生碰撞的文件
```

经过这一层之后，不是所有大文件都会进入 full hash。只有 partial hash 仍然碰撞的大文件，才会进入下一阶段。

```text
largeCandidates
  -> partial hash
  -> partialCollisions
  -> full hash
```

这就是 partial hash 漏斗的作用：在 full hash 之前，用更低成本的局部读取尽量缩小大文件候选集合。

不过，后续 full hash 阶段还要注意任务粒度。V3 曾经把 batch 策略统一应用到大文件，导致一个 worker 可能一次拿到多个大文件，形成长尾任务。V4 在引入 partial hash 的同时，也修复了这个调度问题：大文件进入 full hash 时使用 `Batch Size = 1`，让线程池可以更好地做动态负载均衡。

因此，V4 实际上做了两件事：

```text
第一：
  对大文件增加 head + tail partial hash，减少无效 full hash

第二：
  对真正需要 full hash 的大文件使用更细粒度调度，避免大文件 batch 长尾
```

这两个点要分开理解。

## Benchmark 结果与分析

为了验证 partial hash 是否真的减少了无效 I/O，benchmark 中设计了几个大文件专项 workload：

```text
large_same_size_different_content:
  大文件大小相同，但内容不同

large_same_size_same_prefix_different_tail:
  大文件大小相同，前缀相同，但尾部不同

large_duplicates:
  大文件真实重复
```

这些 workload 分别验证不同问题：

```text
大小相同但内容不同：
  看 partial hash 能否快速排除非重复大文件

前缀相同但尾部不同：
  看 head + tail 是否比只看 head 更稳

真实重复大文件：
  看 partial hash 是否会错误跳过 full hash，以及真实重复时是否仍然必须完整验证
```

以 8 线程结果为主要口径，V2 和 V4 的对比如下：

| Workload                                     | V2      | V4      | 结果     |
| -------------------------------------------- | ------- | ------- | -------- |
| `large_same_size_different_content`          | 76.9 ms | 1.93 ms | 约 39.8x |
| `large_same_size_same_prefix_different_tail` | 77.1 ms | 1.97 ms | 约 39.1x |
| `large_duplicates`                           | 77.4 ms | 78.5 ms | 基本持平 |
| `mixed_realistic`                            | 461 ms  | 358 ms  | 约 1.29x |

第一组 `large_same_size_different_content` 从 `76.9 ms` 降到 `1.93 ms`，提升约 `39.8x`。这说明对于大小相同但内容不同的大文件，head + tail partial hash 可以在读取少量数据后就排除掉它们，避免完整读取 8MiB 文件。

第二组 `large_same_size_same_prefix_different_tail` 从 `77.1 ms` 降到 `1.97 ms`，提升约 `39.1x`。这个结果非常关键，因为它证明只读 head 不够，head + tail 的设计是有意义的。如果两个文件前缀相同但尾部不同，只读头部可能无法区分，而读取尾部可以更早排除。

第三组 `large_duplicates` 基本持平，从 `77.4 ms` 到 `78.5 ms`。这不是优化失败，而是符合预期。因为这些文件真的重复，partial hash 不应该把它们排除。它们最终仍然必须进入 full hash，完整读取文件内容，才能保证结果正确。

因此，V4 的结论应该这样理解：

```text
partial hash 对非重复大文件非常有效；
对真实重复大文件不会天然变快；
这恰好说明它优化的是“无效 I/O”，而不是所有大文件。
```

`mixed_realistic` 从 `461 ms` 降到 `358 ms`，约 `1.29x`。这个提升没有专项大文件 workload 那么夸张，因为混合场景中同时包含小文件、小文件调度成本、大文件 partial hash、真实重复文件等多种因素。但它说明在更接近真实目录的场景中，partial hash 仍然能带来净收益。

还有一个容易误读的点：V4 相比 V3 的提升非常大，但不能直接把这个提升全部归因于 partial hash。因为 V3 把 batch 策略误用于大文件，导致大文件任务出现长尾，这是一个实现边界问题。V4 的提升包含两部分：

```text
修复 V3 大文件 batch 长尾回归
+
引入 head + tail partial hash
```

所以评估 partial hash 的真实净收益时，更应该看稳定基线 `V2 -> V4`，而不是只看 `V3 -> V4`。

## 小结

第 5 层大文件 partial hash 漏斗的核心是减少无效 I/O。

经过 size 过滤后，大小相同的大文件仍然可能内容完全不同。如果直接 full hash，就会读取大量最终没有必要读取的数据。partial hash 在 full hash 之前增加了一层低成本过滤：先读取文件头部和尾部少量数据，计算局部 hash；如果局部 hash 不同，就直接跳过 full hash；如果局部 hash 相同，再进入完整内容 hash。

这个设计保证了正确性，因为 partial hash 只用于排除明显不同的文件，不用于最终判断重复。最终重复判断仍然依赖 full hash。

Benchmark 结果证明，这一层对非重复大文件非常有效。`large_same_size_different_content` 和 `large_same_size_same_prefix_different_tail` 相对 V2 都取得了约 40 倍提升；而 `large_duplicates` 基本持平，说明真实重复大文件仍然必须完整验证，这是符合预期的。

因此，这一层的定位可以概括为：

```text
小文件优化关注固定成本；
大文件优化关注无效 I/O；
partial hash 是大文件 full hash 之前的一层低成本过滤漏斗。
```