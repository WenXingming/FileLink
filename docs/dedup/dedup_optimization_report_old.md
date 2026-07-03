# dedup 性能优化与 Benchmark 报告

本文档基于 [dedup_benchmark_record.md](dedup_benchmark_record.md) 中按 commit 重新运行的 Google Benchmark 结果，以及当前工作区补充实验，整理 `dedup` 文件去重工具的性能优化过程。

这份报告重点不是罗列所有数字，而是把每一步优化讲清楚：

- 为什么要做这个优化（怀疑的瓶颈是什么）。
- 怎么做这个优化（优化和实现思路）。
- benchmark 结果是否支持这个优化。

历史版本的原始终端输出保留在 [dedup_benchmark_record.md](dedup_benchmark_record.md)。本文同时保留当前工作区补充实验的整理结果，只讨论 `_mean` 数据，并以 8 线程结果作为主要口径。

---

## 结论先行

这轮优化可以分成两条主线看：小文件优化和大文件优化。

第一条是大量小文件场景下优化，核心目标是降低“每个文件都会发生的固定成本”。V2 的 buffer 清零优化本质上是通用优化，但它对小文件尤其明显：小文件有效数据少、文件数量多，buffer 初始化、buffer 分配、任务调度、future 管理这类固定成本占比更高。V2 之后，基础 workload 的 8 线程耗时普遍降低到原来的约三分之一。V3 的小文件 batch 也是沿着这条主线继续尝试，只是当前 benchmark 没有证明明显收益。V6 继续补了一次更细的 buffer 优化：按实际读取量缩小 buffer，并用 `thread_local` 在线程内复用 scratch buffer；补充 benchmark 显示它对部分基础小文件 workload 有小幅收益，但不是新的数量级提升。

第二条是大文件漏斗优化，核心目标是减少无效 I/O。V4 增加 head+tail partial hash 后，对“大小相同但内容不同”的大文件非常有效。相对 V2，`large_same_size_different_content` 从 `76.9 ms` 降到 `1.93 ms`，约 `39.8x`；`large_same_size_same_prefix_different_tail` 从 `77.1 ms` 降到 `1.97 ms`，约 `39.1x`。

报告里最容易误读的是 V3 到 V4 的巨大提升。V3 把 batch 策略统一应用到了大文件，导致大文件任务出现长尾，这是实现边界问题。V4 的部分提升来自修复这个回归，所以评估 V4 的真实净收益时，更应该看 `V2 -> V4`，而不是只看 `V3 -> V4`。

可以用一句话概括：

> 这个优化不是简单加线程，而是按文件类型拆瓶颈：小文件主线降低每文件固定成本，大文件主线减少无效 I/O，真实重复大文件保持细粒度调度避免长尾。

---

## 测试环境与运行方法

### 机器环境

| 项目 | 值 |
| --- | --- |
| 主机 | `wxm-Precision-7920-Tower` |
| CPU | 24 logical CPUs, 3500 MHz |
| L1 Data Cache | 32 KiB x12 |
| L1 Instruction Cache | 32 KiB x12 |
| L2 Cache | 1024 KiB x12 |
| L3 Cache | 16896 KiB x1 |
| Benchmark 日期 | 2026-06-25 |
| 数据生成目录 | `/tmp/threadpool_dedup_bench_XXXXXX` |

各版本记录：

| 版本 | Commit | 时间 | Load Average |
| --- | --- | --- | --- |
| V1 基线 | `ad7f5ba10f6403cab0ccab2b753c3e8641f016cd` | 2026-06-25T10:02:06+08:00 | 2.23, 1.29, 0.94 |
| V2 Buffer 优化 | `d39a756702fe1917ab851c1945cdbb8af41dcac0` | 2026-06-25T10:07:42+08:00 | 4.36, 2.19, 1.40 |
| V3 小文件批量切片 | `b4fc4af268eef56030fe0845a1df7016aeb88648` | 2026-06-25T10:23:30+08:00 | 1.55, 3.09, 2.70 |
| V4 大文件首尾哈希 | `4972c07ebd6e209486bebe4ba98a47c16ebf032c` | 2026-06-25T10:37:27+08:00 | 1.23, 1.35, 1.86 |
| V5 硬链接折叠与 I/O 局部性 | `17d2aeac4c3495d26b64607c66bacaaa89a067e1` | 未单独 benchmark | - |
| V6 Buffer right-sizing 与线程局部复用 | 当前工作区补充实验 | 2026-06-25T21:19:12+08:00 / 2026-06-25T22:18:16+08:00 | 1.28, 1.25, 1.08 / 2.78, 1.30, 1.16 |

每次运行均提示 CPU scaling enabled，所以本文关注趋势和数量级，不把小幅波动解释为确定结论。

V6 的数据来自当前工作区的两次微步实验，不是历史 commit 序列中的独立提交。因此它适合验证“小文件 buffer 分配是否还有优化空间”，但不应把所有大文件数字都直接归因到这次 buffer 改动。

### 运行命令

构建：

```bash
cmake -S . -B build-bench \
  -DFILELINK_BUILD_BENCHMARKS=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DFETCHCONTENT_SOURCE_DIR_THREADPOOL=/home/wxm/ThreadPool

cmake --build build-bench
```

运行：

```bash
./build-bench/benchmarks/dedup/filelink-dedup-benchmark \
  --benchmark_min_time=1.0 \
  --benchmark_repetitions=3 \
  --benchmark_report_aggregates_only=true
```

需要机器可读结果时，可以输出 JSON：

```bash
./build-bench/benchmarks/dedup/filelink-dedup-benchmark \
  --benchmark_min_time=1.0 \
  --benchmark_repetitions=3 \
  --benchmark_report_aggregates_only=true \
  --benchmark_format=json \
  > benchmarks/dedup/results/latest.json
```

### 指标口径

| 字段 | 含义 |
| --- | --- |
| `Time` | wall time，用户真实等待时间，本文主要看它 |
| `CPU` | CPU 实际执行时间；如果明显小于 `Time`，说明 I/O 等待很多 |
| `_mean` | 3 次重复运行的平均值 |
| `bytes_per_second` | Google Benchmark 按逻辑文件大小计算的吞吐 |
| `items_per_second` | 每秒处理文件数 |

注意：V4 之后的 partial hash 场景中，`bytes_per_second` 是逻辑吞吐，不是实际读盘吞吐。程序可能只读每个大文件的头尾几 KiB，但 Google Benchmark 仍按完整 8MiB 文件计入逻辑数据量。

---

## Benchmark 设计

### 设计原则

文件去重的瓶颈不是单一的，不同文件形态对应不同问题：

| 场景 | 主要瓶颈 |
| --- | --- |
| 大量小文件 | open/read/close、任务调度、future 管理 |
| 中等文件 | I/O 等待与 hash 计算混合 |
| 大文件同大小但内容不同 | 无效 full hash 造成大量 I/O |
| 大文件真实重复 | 必须 full hash，考验并发和负载均衡 |
| 混合目录 | 小文件调度、大文件漏斗、负载均衡同时出现 |

因此 benchmark 分成两类：

- 基础 workload：V1 到 V4 都存在，用于看小文件/中等文件的历史吞吐演进；V6 作为当前工作区补充实验加入汇总。
- 专项 workload：从 V2 开始加入，用于验证具体优化点。

### Workload 列表

| Workload | 文件数 | 文件大小 | 重复比例 | 目的 |
| --- | ---: | ---: | ---: | --- |
| `100_files_4KiB` | 100 | 4 KiB | 20% | 小规模 sanity |
| `1000_files_4KiB` | 1000 | 4 KiB | 20% | 小文件基础吞吐 |
| `10000_files_4KiB` | 10000 | 4 KiB | 20% | many-small-files |
| `1000_files_256KiB` | 1000 | 256 KiB | 20% | 中等文件吞吐 |
| `many_small_unique_same_size` | 10000 | 4 KiB | 0% | size 过滤失效，必须 hash 全部小文件 |
| `many_small_duplicates` | 10000 | 4 KiB | 100% | 小文件重复场景 |
| `large_same_size_different_content` | 20 | 8 MiB | 0% | 大文件同大小但内容不同 |
| `large_same_size_same_prefix_different_tail` | 20 | 8 MiB | 0% | 大文件同前缀但尾部不同 |
| `large_duplicates` | 20 | 8 MiB | 100% | 大文件真实重复 |
| `mixed_realistic` | 1000 | 混合 | 20% | 模拟真实混合目录 |

---

## 优化路线

最终优化方向是一条漏斗：

```text
扫描文件
  -> 按 size 分组，大小唯一直接跳过
  -> hardlink 折叠，避免同一物理文件重复 I/O
  -> 小文件：直接 full hash，可以合批
  -> 大文件：先做 head+tail partial hash
  -> partial hash 仍碰撞：再 full hash
  -> 按 (size, full hash) 输出重复组
```

核心原则：

- 大文件优化：少读。
- 小文件优化：少调度、少固定开销。
- 真实重复大文件：无法少读，只能靠并发和细粒度调度避免长尾。
- 文件系统优化：识别 hardlink，按 inode 改善访问局部性。

---

## 版本演进与结果



### V2：Buffer 避免 zero-initialization

#### 做了什么

原来每次 hash 文件都会创建：

```cpp
std::vector<char> buffer(bufferSize_);
```

这会分配并清零 buffer。对 4KiB 小文件来说，如果 buffer 很大，清零成本可能比真正读取文件还贵。

V2 改为：

```cpp
std::unique_ptr<char[]> buffer(new char[bufferSize_]);
```

它只分配，不做无意义清零，因为后续 `read` 会立刻覆盖这块内存。

#### 全量结果

单位：`Time mean`。最后一列比较 8 线程；V2 新增的专项 workload 标记为 `新增`。

| Workload | 1 线程 | 2 线程 | 4 线程 | 8 线程 | 8线程对比 V1 |
| --- | ---: | ---: | ---: | ---: | ---: |
| `100_files_4KiB` | 2.40 ms | 1.40 ms | 0.867 ms | 0.749 ms | 快 3.22x |
| `1000_files_4KiB` | 22.2 ms | 12.6 ms | 7.92 ms | 6.83 ms | 快 3.24x |
| `10000_files_4KiB` | 129 ms | 105 ms | 83.3 ms | 70.2 ms | 快 3.05x |
| `1000_files_256KiB` | 169 ms | 91.8 ms | 49.8 ms | 32.1 ms | 快 3.83x |
| `many_small_unique_same_size` | 332 ms | 220 ms | 147 ms | 117 ms | 新增 |
| `many_small_duplicates` | 334 ms | 224 ms | 153 ms | 121 ms | 新增 |
| `large_same_size_different_content` | 479 ms | 243 ms | 124 ms | 76.9 ms | 新增 |
| `large_same_size_same_prefix_different_tail` | 481 ms | 243 ms | 124 ms | 77.1 ms | 新增 |
| `large_duplicates` | 482 ms | 244 ms | 124 ms | 77.4 ms | 新增 |
| `mixed_realistic` | 3319 ms | 1680 ms | 862 ms | 461 ms | 新增 |

#### 解读

V2 是整轮优化里最稳定、最容易解释的一步。它不改变去重逻辑，只去掉每个文件都会触发的无意义清零成本。

基础 workload 的 8 线程结果普遍提升约 `3x`。这说明当文件很多时，单个文件上的固定开销会被放大，哪怕每次只浪费一点点，也会在全局结果里变得很明显。

---

### V3：小文件批量切片

#### 做了什么

V2 后，我们怀疑小文件瓶颈从 buffer 初始化转移到了任务调度。

旧模型：

```text
1 个文件 = 1 个线程池任务 = 1 个 future
```

V3 改成批量切片：

```text
一批文件 = 1 个线程池任务 = 1 个 future
```

目标是减少任务队列争用和 future 管理成本。这个优化本来只应该服务于小文件；如果把同样的 batch 策略用于大文件，就会变成负载均衡问题。

#### 全量结果

单位：`Time mean`。最后一列比较 8 线程。

| Workload | 1 线程 | 2 线程 | 4 线程 | 8 线程 | 8线程对比 V2 |
| --- | ---: | ---: | ---: | ---: | ---: |
| `100_files_4KiB` | 2.77 ms | 2.79 ms | 2.84 ms | 2.76 ms | 慢 3.68x |
| `1000_files_4KiB` | 19.2 ms | 19.0 ms | 11.2 ms | 11.1 ms | 慢 1.63x |
| `10000_files_4KiB` | 139 ms | 111 ms | 83.5 ms | 70.1 ms | 基本持平 |
| `1000_files_256KiB` | 170 ms | 115 ms | 94.5 ms | 95.2 ms | 慢 2.97x |
| `many_small_unique_same_size` | 289 ms | 215 ms | 165 ms | 119 ms | 慢 1.02x |
| `many_small_duplicates` | 312 ms | 222 ms | 166 ms | 137 ms | 慢 1.13x |
| `large_same_size_different_content` | 480 ms | 514 ms | 511 ms | 510 ms | 慢 6.63x，属于大文件误用 batch 的实现回归 |
| `large_same_size_same_prefix_different_tail` | 483 ms | 512 ms | 515 ms | 512 ms | 慢 6.64x，属于大文件误用 batch 的实现回归 |
| `large_duplicates` | 481 ms | 518 ms | 517 ms | 516 ms | 慢 6.67x，属于大文件误用 batch 的实现回归 |
| `mixed_realistic` | 3323 ms | 1892 ms | 1691 ms | 1505 ms | 慢 3.26x，受大文件回归拖累 |

#### 解读

V3 的价值不是“证明小文件 batch 一定更快”，而是帮助我们划清策略边界。

只看小文件，V3 没有明显收益：

- `10000_files_4KiB` 基本持平：`70.2 ms -> 70.1 ms`。
- `many_small_unique_same_size` 略慢：`117 ms -> 119 ms`。
- `many_small_duplicates` 略慢：`121 ms -> 137 ms`。

这说明在当前 `/tmp` 和 Page Cache 较热的 benchmark 环境下，单文件任务调度不是主要瓶颈。文件数量更大、冷缓存、真实磁盘或系统调用更重的环境下，batch 仍可能有收益，但这组数据没有证明出来。

V3 暴露出的真正问题是：

```text
统一 batch 对小文件可能合理，但对大文件非常危险。
```

如果一个 worker 拿到 64 个大文件，它会长时间运行，其他线程很快空闲，整体耗时被最慢的 batch 拖住。因此，大文件退化不应归因于“小文件切片思路”本身，而应归因于实现时没有把小文件和大文件的调度策略拆开。

---

### V4：大文件首尾哈希与细粒度调度

#### 做了什么

V4 同时做了两件事。

第一，修复 V3 暴露的大文件调度问题：

```text
小文件：可以 batch，减少任务开销
大文件：Batch=1，保持线程池负载均衡
```

第二，新增大文件漏斗：

```text
size 相同
  -> 读取头部 2KiB + 尾部 2KiB，计算 partial hash
  -> partial hash 不同：跳过 full hash
  -> partial hash 相同：进入 full hash
```

这个设计的目标很明确：对同大小但内容不同的大文件，尽早用少量 I/O 排除，避免从头到尾 full hash。

#### 全量结果

单位：`Time mean`。`8线程对比 V3` 用来观察 V4 修复 V3 回归的幅度；`8线程对比 V2` 更接近 V4 相对稳定基线的真实净收益。

| Workload | 1 线程 | 2 线程 | 4 线程 | 8 线程 | 8线程对比 V3 | 8线程对比 V2 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `100_files_4KiB` | 2.51 ms | 2.84 ms | 2.83 ms | 2.79 ms | 基本持平 | 慢 3.72x |
| `1000_files_4KiB` | 22.8 ms | 19.3 ms | 11.1 ms | 11.2 ms | 基本持平 | 慢 1.64x |
| `10000_files_4KiB` | 137 ms | 99.7 ms | 84.7 ms | 69.8 ms | 基本持平 | 基本持平 |
| `1000_files_256KiB` | 176 ms | 97.1 ms | 58.5 ms | 37.1 ms | 快 2.57x | 慢 1.16x |
| `many_small_unique_same_size` | 309 ms | 219 ms | 145 ms | 130 ms | 慢 1.09x | 慢 1.11x |
| `many_small_duplicates` | 316 ms | 222 ms | 175 ms | 132 ms | 基本持平 | 慢 1.09x |
| `large_same_size_different_content` | 1.02 ms | 1.71 ms | 1.86 ms | 1.93 ms | 快 264x，含修复回归 | 快 39.8x，真实 partial hash 收益 |
| `large_same_size_same_prefix_different_tail` | 0.995 ms | 1.59 ms | 1.82 ms | 1.97 ms | 快 260x，含修复回归 | 快 39.1x，真实 partial hash 收益 |
| `large_duplicates` | 485 ms | 245 ms | 126 ms | 78.5 ms | 快 6.57x，主要是修复回归 | 基本持平 |
| `mixed_realistic` | 2538 ms | 1287 ms | 671 ms | 358 ms | 快 4.20x，含修复回归 | 快 1.29x，真实净收益 |

#### 解读

V4 的结果需要分三类解释。

非重复大文件极大加速。`large_same_size_different_content` 相比 V2 从 `76.9 ms` 降到 `1.93 ms`，约 `39.8x`。原因是这些文件虽然 size 相同，但头尾局部哈希不同，所以不需要 full hash。

同前缀不同尾的大文件也能被过滤。`large_same_size_same_prefix_different_tail` 相比 V2 从 `77.1 ms` 降到 `1.97 ms`，约 `39.1x`。这个 workload 证明只读头部不够，head+tail 比只读 head 更稳。

真实重复大文件不会天然变快。`large_duplicates` 相比 V3 从 `516 ms` 降到 `78.5 ms`，但相比 V2 的 `77.4 ms` 基本持平。因为文件真的重复，partial hash 不能排除，最终仍然必须 full hash。这里 V4 的价值是修复 V3 的大文件长尾，而不是让真实重复大文件少读数据。

`mixed_realistic` 相比 V3 从 `1505 ms` 降到 `358 ms`，包含修复回归；相比 V2 从 `461 ms` 降到 `358 ms`，约 `1.29x`，这才是 V4 在混合场景里的真实净收益。


### V6：Buffer right-sizing 与线程局部复用

#### 做了什么

V6 是 V2 buffer 优化的延续，目标仍然是减少每个文件都会付出的固定成本。

V2 已经把 `std::vector<char>(bufferSize_)` 改成了 `new char[bufferSize_]`，避免每次 hash 前清零整块 buffer。但后续实现里仍然存在两个细节问题：

- 对 4KiB 小文件，仍可能申请默认 1MiB buffer，buffer 尺寸明显大于实际读取量。
- 每次 `hash_file()` / `hash_partial_file()` 调用都会重新分配临时 buffer。

V6 拆成两个微步验证：

```text
微步 1：Buffer right-sizing
  full hash: buffer = min(bufferSize, file.size)
  partial hash: buffer = min(bufferSize, maxBytes 或 head/tail chunk)

微步 2：thread_local scratch buffer
  每个 worker 线程保留自己的 vector<char>
  容量不足时扩容，容量足够时直接复用
```

这里没有引入通用内存池。原因是这个场景只需要“每个工作线程复用一块读缓冲”，`thread_local` 更简单，也避免了全局池的锁、借还协议和生命周期管理。

#### 补充结果

单位：`Time mean`。表中列出 8 线程结果；`V6 right-sizing` 是微步 1，`V6 thread_local` 是微步 2。

| Workload | V4 | V6 right-sizing | V6 thread_local | 解读 |
| --- | ---: | ---: | ---: | --- |
| `100_files_4KiB` | 2.79 ms | 2.20 ms | 2.18 ms | 小规模短任务噪声较大，只能看作轻微改善 |
| `1000_files_4KiB` | 11.2 ms | 7.41 ms | 7.26 ms | 有比较明显改善，说明小文件固定成本仍可压缩 |
| `10000_files_4KiB` | 69.8 ms | 65.0 ms | 66.1 ms | 小幅改善，thread_local 相比 right-sizing 没有继续变好 |
| `1000_files_256KiB` | 37.1 ms | 26.9 ms | 27.1 ms | 中等文件也受益，但这组数据可能包含当前工作区其他改动影响 |
| `many_small_unique_same_size` | 130 ms | 132 ms | 135 ms | 没有收益，甚至略慢 |
| `many_small_duplicates` | 132 ms | 132 ms | 136 ms | 没有收益，thread_local 略慢 |
| `mixed_realistic` | 358 ms | 111 ms | 111 ms | 不能直接归因于 buffer；当前工作区已包含后续改动 |

#### 解读

V6 的结论要分开看。

`Buffer right-sizing` 是合理的。对小文件来说，按实际读取量缩小 buffer 能减少无意义的大块分配和缓存/TLB 压力。`1000_files_4KiB` 从 V4 的 `11.2 ms` 降到 `7.41 ms`，`10000_files_4KiB` 从 `69.8 ms` 降到 `65.0 ms`，方向符合预期。

`thread_local` 复用没有证明出稳定的额外收益。它相比 right-sizing 后的结果基本持平：`1000_files_4KiB` 从 `7.41 ms` 到 `7.26 ms`，但 `10000_files_4KiB` 从 `65.0 ms` 到 `66.1 ms`，`many_small_duplicates` 从 `132 ms` 到 `136 ms`。这说明在当前 benchmark 下，分配次数未必还是主瓶颈；open/read/close、文件系统缓存、任务调度和 benchmark 噪声都可能占更大比例。

因此，V6 适合这样定位：

```text
它是一次小文件固定成本的收口优化，不是新的架构级提速。
```

实现上保留 `thread_local` 仍然有工程合理性：它无锁、线程安全、代码量小，可以避免未来更大文件数或更高并发下的频繁分配。但 benchmark 报告里不能把它包装成“内存池带来显著性能飞跃”。

---

## 汇总结果

### 小文件与基础 workload 演进

只看基础 workload，比较 8 线程。V6 是当前工作区补充实验，放在同表里帮助观察趋势，但不要把它当成严格历史 commit A/B：

| Workload | V1 | V2 | V3 | V4 | V6 right-sizing | V6 thread_local | 最终结论 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `100_files_4KiB` | 2.41 ms | 0.749 ms | 2.76 ms | 2.79 ms | 2.20 ms | 2.18 ms | 短任务噪声大，不作为核心结论 |
| `1000_files_4KiB` | 22.1 ms | 6.83 ms | 11.1 ms | 11.2 ms | 7.41 ms | 7.26 ms | V6 补充实验显示固定成本仍有压缩空间 |
| `10000_files_4KiB` | 214 ms | 70.2 ms | 70.1 ms | 69.8 ms | 65.0 ms | 66.1 ms | V6 小幅改善，但不是数量级提升 |
| `1000_files_256KiB` | 123 ms | 32.1 ms | 95.2 ms | 37.1 ms | 26.9 ms | 27.1 ms | V6 有改善，但可能包含当前工作区其他改动影响 |

这张表主要回答的是：

```text
小文件/中等文件的基础吞吐有没有变好？
```

答案是：有，主要来自 V2 的 buffer 优化。V2 降低的是每文件固定成本，所以对大量小文件尤其明显。V3 在部分基础 workload 上有波动；其中大文件相关退化属于 batch 策略应用范围过宽的实现问题，V4 对此做了修复。V6 继续沿着 buffer 固定成本做收口优化，`1000_files_4KiB` 和 `10000_files_4KiB` 有改善，但 `thread_local` 相比 right-sizing 没有稳定额外收益。

### 大文件与专项 workload 验证

只看 V2/V3/V4 都存在的专项 workload，比较 8 线程：

| Workload | V2 | V3 | V4 | 说明 |
| --- | ---: | ---: | ---: | --- |
| `many_small_unique_same_size` | 117 ms | 119 ms | 130 ms | 小文件 batching 在热缓存下未证明收益 |
| `many_small_duplicates` | 121 ms | 137 ms | 132 ms | 小文件结果波动，未证明收益 |
| `large_same_size_different_content` | 76.9 ms | 510 ms | 1.93 ms | V3 退化是实现问题；V2->V4 约 39.8x，才是 partial hash 净收益 |
| `large_same_size_same_prefix_different_tail` | 77.1 ms | 512 ms | 1.97 ms | V2->V4 约 39.1x，证明 head+tail 有效 |
| `large_duplicates` | 77.4 ms | 516 ms | 78.5 ms | V4 修复 V3 长尾；相对 V2 基本持平 |
| `mixed_realistic` | 461 ms | 1505 ms | 358 ms | V4 修复 V3 回归；相对 V2 约 1.29x |

这张表主要回答的是：

```text
大文件优化有没有减少无效 I/O？
```

答案是：

- 小文件 batching 在当前 benchmark 中没有证明出明显收益。
- V3 的大文件下降属于实现边界问题：统一 batch 误用于大文件。
- V4 的 partial hash 对非重复大文件非常有效，真实收益应看 V2->V4。
- 真实重复大文件仍然必须 full hash，所以相对 V2 基本持平是符合预期的。
- 混合场景最终 V4 最好；相对 V2 的真实净收益约 `1.29x`。

V6 的大文件和混合场景结果没有放进这张表作为主结论。原因是 V6 来自当前工作区补充实验，运行环境和代码状态已经不同于历史 V2/V3/V4 commit；它更适合验证 buffer 相关的小文件固定成本，而不是重新评价 partial hash 的架构收益。

---

## 叙述讲法

可以按下面这条主线讲：

- V1 已经有线程池，benchmark 证明并行 I/O 能带来明显加速，例如 `1000_files_256KiB` 从 `852 ms` 降到 `123 ms`。
- 小文件主线关注每文件固定成本。V2 优化 buffer 分配，避免每个文件 hash 前无意义清零；这个优化是通用的，但对小文件和大量文件特别有效，基础 workload 普遍提升约 `3x`。
- V3 继续沿着小文件主线尝试 batch，想减少任务和 future 数量。但 benchmark 说明，在当前热缓存 `/tmp` 环境下，小文件 batch 没有证明明显收益；同时它暴露了一个实现边界：大文件不能统一 batch。
- 大文件主线关注减少无效 I/O。V4 把小文件和大文件调度拆开，大文件使用 Batch=1 避免长尾，并加入 head+tail partial hash。相对 V2，非重复大文件场景提升约 `40x`；真实重复大文件相对 V2 基本持平，符合“必须 full hash”的预期。
- V5 引入 `device/inode`，用于 hardlink 折叠和 I/O 局部性优化，但还需要专门 benchmark 验证。
- V6 回到小文件固定成本，做 buffer right-sizing 和 `thread_local` scratch buffer。结论是 right-sizing 合理，`thread_local` 工程上可以保留，但 benchmark 没有证明它带来稳定的额外加速。

更短的版本：

> 我把文件去重拆成两条优化主线：小文件主要优化每文件固定成本，大文件主要减少无效 I/O。V2 通过避免 buffer 清零降低固定成本，这个优化是通用的，但对大量小文件尤其明显，基础 workload 提升约 3 倍；V6 进一步做 buffer right-sizing 和线程局部复用，属于小文件固定成本的收口优化，收益小但方向合理。V4 通过 head+tail partial hash 过滤非重复大文件，相对稳定基线 V2 提升约 40 倍。V3 的大文件退化不是小文件 batch 思路的问题，而是 batch 策略误用于大文件，V4 已经通过大文件单独调度修复。

---

## 后续 Benchmark 建议

| Benchmark | 目的 |
| --- | --- |
| `hardlink_heavy` | 验证 hardlink 折叠能减少 full hash 次数 |
| `cold_cache_many_small` | 在真实磁盘冷缓存下验证 inode 排序 |
| `partial_hash_false_positive` | 构造头尾相同但中间不同的大文件，评估 partial hash 假阳性 |
| `large_file_batch_sweep` | 对大文件 batch size 做 1/2/4/8 对比，验证 Batch=1 是否最优 |
| `buffer_size_sweep` | 对 64KiB/256KiB/1MiB/8MiB buffer 做对比 |
| `buffer_reuse_ablation` | 在同一代码基线上分别测试原始分配、right-sizing、thread_local，隔离 V6 的真实收益 |
