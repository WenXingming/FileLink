> 本文保留迁移前在 FileSystemTools 中执行 benchmark 时的原始命令和输出路径，用于保证历史评测记录可追溯。

## V1 (基线)：最初的多线程 Hash

Commit Hash：ad7f5ba10f6403cab0ccab2b753c3e8641f016cd

```
wxm@wxm-Precision-7920-Tower:~/FileSystemTools$ ./build-bench/benchmark/dedup/dedupBenchmark \
  --benchmark_min_time=1.0 \
  --benchmark_repetitions=3 \
  --benchmark_report_aggregates_only=true
2026-06-25T10:02:06+08:00
Running ./build-bench/benchmark/dedup/dedupBenchmark
Run on (24 X 3500 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x12)
  L1 Instruction 32 KiB (x12)
  L2 Unified 1024 KiB (x12)
  L3 Unified 16896 KiB (x1)
Load Average: 2.23, 1.29, 0.94
***WARNING*** CPU scaling is enabled, the benchmark real time measurements may be noisy and will incur extra overhead.
---------------------------------------------------------------------------------------------------------------------------
Benchmark                                                                 Time             CPU   Iterations UserCounters...
---------------------------------------------------------------------------------------------------------------------------
DuplicateFinder/100_files_4KiB/threads_1/100/4096/1_mean               9.92 ms         2.75 ms            3 bytes_per_second=142.942M/s items_per_second=36.5933k/s
DuplicateFinder/100_files_4KiB/threads_1/100/4096/1_median             9.92 ms         2.84 ms            3 bytes_per_second=137.326M/s items_per_second=35.1554k/s
DuplicateFinder/100_files_4KiB/threads_1/100/4096/1_stddev            0.257 ms        0.243 ms            3 bytes_per_second=13.2432M/s items_per_second=3.39025k/s
DuplicateFinder/100_files_4KiB/threads_1/100/4096/1_cv                 2.59 %          8.84 %             3 bytes_per_second=9.26% items_per_second=9.26%
DuplicateFinder/100_files_4KiB/threads_2/100/4096/2_mean               6.27 ms         2.53 ms            3 bytes_per_second=154.222M/s items_per_second=39.4809k/s
DuplicateFinder/100_files_4KiB/threads_2/100/4096/2_median             6.32 ms         2.55 ms            3 bytes_per_second=152.919M/s items_per_second=39.1474k/s
DuplicateFinder/100_files_4KiB/threads_2/100/4096/2_stddev            0.169 ms        0.057 ms            3 bytes_per_second=3.51783M/s items_per_second=900.565/s
DuplicateFinder/100_files_4KiB/threads_2/100/4096/2_cv                 2.70 %          2.26 %             3 bytes_per_second=2.28% items_per_second=2.28%
DuplicateFinder/100_files_4KiB/threads_4/100/4096/4_mean               3.65 ms         1.87 ms            3 bytes_per_second=208.671M/s items_per_second=53.4199k/s
DuplicateFinder/100_files_4KiB/threads_4/100/4096/4_median             3.64 ms         1.87 ms            3 bytes_per_second=209.386M/s items_per_second=53.6028k/s
DuplicateFinder/100_files_4KiB/threads_4/100/4096/4_stddev            0.044 ms        0.018 ms            3 bytes_per_second=2.01229M/s items_per_second=515.145/s
DuplicateFinder/100_files_4KiB/threads_4/100/4096/4_cv                 1.19 %          0.97 %             3 bytes_per_second=0.96% items_per_second=0.96%
DuplicateFinder/100_files_4KiB/threads_8/100/4096/8_mean               2.41 ms         1.35 ms            3 bytes_per_second=290.867M/s items_per_second=74.4621k/s
DuplicateFinder/100_files_4KiB/threads_8/100/4096/8_median             2.38 ms         1.34 ms            3 bytes_per_second=292.272M/s items_per_second=74.8217k/s
DuplicateFinder/100_files_4KiB/threads_8/100/4096/8_stddev            0.272 ms        0.130 ms            3 bytes_per_second=27.7447M/s items_per_second=7.10264k/s
DuplicateFinder/100_files_4KiB/threads_8/100/4096/8_cv                11.30 %          9.65 %             3 bytes_per_second=9.54% items_per_second=9.54%
DuplicateFinder/1000_files_4KiB/threads_1/1000/4096/1_mean              121 ms         28.1 ms            3 bytes_per_second=138.997M/s items_per_second=35.5833k/s
DuplicateFinder/1000_files_4KiB/threads_1/1000/4096/1_median            121 ms         28.2 ms            3 bytes_per_second=138.631M/s items_per_second=35.4895k/s
DuplicateFinder/1000_files_4KiB/threads_1/1000/4096/1_stddev          0.731 ms        0.187 ms            3 bytes_per_second=948.506k/s items_per_second=237.127/s
DuplicateFinder/1000_files_4KiB/threads_1/1000/4096/1_cv               0.61 %          0.66 %             3 bytes_per_second=0.67% items_per_second=0.67%
DuplicateFinder/1000_files_4KiB/threads_2/1000/4096/2_mean             65.3 ms         20.7 ms            3 bytes_per_second=188.591M/s items_per_second=48.2794k/s
DuplicateFinder/1000_files_4KiB/threads_2/1000/4096/2_median           65.2 ms         20.6 ms            3 bytes_per_second=189.86M/s items_per_second=48.6042k/s
DuplicateFinder/1000_files_4KiB/threads_2/1000/4096/2_stddev           1.18 ms         1.03 ms            3 bytes_per_second=9.24956M/s items_per_second=2.36789k/s
DuplicateFinder/1000_files_4KiB/threads_2/1000/4096/2_cv               1.81 %          4.96 %             3 bytes_per_second=4.90% items_per_second=4.90%
DuplicateFinder/1000_files_4KiB/threads_4/1000/4096/4_mean             38.1 ms         17.5 ms            3 bytes_per_second=227.426M/s items_per_second=58.221k/s
DuplicateFinder/1000_files_4KiB/threads_4/1000/4096/4_median           37.9 ms         17.3 ms            3 bytes_per_second=226.213M/s items_per_second=57.9106k/s
DuplicateFinder/1000_files_4KiB/threads_4/1000/4096/4_stddev           3.42 ms         3.13 ms            3 bytes_per_second=40.2564M/s items_per_second=10.3056k/s
DuplicateFinder/1000_files_4KiB/threads_4/1000/4096/4_cv               8.98 %         17.84 %             3 bytes_per_second=17.70% items_per_second=17.70%
DuplicateFinder/1000_files_4KiB/threads_8/1000/4096/8_mean             22.1 ms         12.1 ms            3 bytes_per_second=327.514M/s items_per_second=83.8436k/s
DuplicateFinder/1000_files_4KiB/threads_8/1000/4096/8_median           21.3 ms         11.2 ms            3 bytes_per_second=347.453M/s items_per_second=88.948k/s
DuplicateFinder/1000_files_4KiB/threads_8/1000/4096/8_stddev           1.91 ms         1.63 ms            3 bytes_per_second=41.0668M/s items_per_second=10.5131k/s
DuplicateFinder/1000_files_4KiB/threads_8/1000/4096/8_cv               8.61 %         13.49 %             3 bytes_per_second=12.54% items_per_second=12.54%
DuplicateFinder/10000_files_4KiB/threads_1/10000/4096/1_mean           1042 ms          235 ms            3 bytes_per_second=166.504M/s items_per_second=42.625k/s
DuplicateFinder/10000_files_4KiB/threads_1/10000/4096/1_median         1038 ms          240 ms            3 bytes_per_second=163M/s items_per_second=41.7279k/s
DuplicateFinder/10000_files_4KiB/threads_1/10000/4096/1_stddev         18.2 ms         8.44 ms            3 bytes_per_second=6.11062M/s items_per_second=1.56432k/s
DuplicateFinder/10000_files_4KiB/threads_1/10000/4096/1_cv             1.75 %          3.59 %             3 bytes_per_second=3.67% items_per_second=3.67%
DuplicateFinder/10000_files_4KiB/threads_2/10000/4096/2_mean            597 ms          190 ms            3 bytes_per_second=205.897M/s items_per_second=52.7097k/s
DuplicateFinder/10000_files_4KiB/threads_2/10000/4096/2_median          612 ms          197 ms            3 bytes_per_second=197.833M/s items_per_second=50.6452k/s
DuplicateFinder/10000_files_4KiB/threads_2/10000/4096/2_stddev         32.1 ms         14.1 ms            3 bytes_per_second=15.9549M/s items_per_second=4.08446k/s
DuplicateFinder/10000_files_4KiB/threads_2/10000/4096/2_cv             5.39 %          7.43 %             3 bytes_per_second=7.75% items_per_second=7.75%
DuplicateFinder/10000_files_4KiB/threads_4/10000/4096/4_mean            355 ms          163 ms            3 bytes_per_second=239.967M/s items_per_second=61.4315k/s
DuplicateFinder/10000_files_4KiB/threads_4/10000/4096/4_median          352 ms          159 ms            3 bytes_per_second=244.963M/s items_per_second=62.7105k/s
DuplicateFinder/10000_files_4KiB/threads_4/10000/4096/4_stddev         8.66 ms         9.63 ms            3 bytes_per_second=13.7705M/s items_per_second=3.52524k/s
DuplicateFinder/10000_files_4KiB/threads_4/10000/4096/4_cv             2.44 %          5.90 %             3 bytes_per_second=5.74% items_per_second=5.74%
DuplicateFinder/10000_files_4KiB/threads_8/10000/4096/8_mean            214 ms          127 ms            3 bytes_per_second=307.655M/s items_per_second=78.7596k/s
DuplicateFinder/10000_files_4KiB/threads_8/10000/4096/8_median          218 ms          127 ms            3 bytes_per_second=307.028M/s items_per_second=78.5991k/s
DuplicateFinder/10000_files_4KiB/threads_8/10000/4096/8_stddev         5.97 ms         5.22 ms            3 bytes_per_second=12.6502M/s items_per_second=3.23846k/s
DuplicateFinder/10000_files_4KiB/threads_8/10000/4096/8_cv             2.78 %          4.10 %             3 bytes_per_second=4.11% items_per_second=4.11%
DuplicateFinder/1000_files_256KiB/threads_1/1000/262144/1_mean          852 ms         31.1 ms            3 bytes_per_second=7.87422G/s items_per_second=32.2528k/s
DuplicateFinder/1000_files_256KiB/threads_1/1000/262144/1_median        851 ms         31.9 ms            3 bytes_per_second=7.65252G/s items_per_second=31.3447k/s
DuplicateFinder/1000_files_256KiB/threads_1/1000/262144/1_stddev       1.09 ms         1.50 ms            3 bytes_per_second=400.306M/s items_per_second=1.60122k/s
DuplicateFinder/1000_files_256KiB/threads_1/1000/262144/1_cv           0.13 %          4.83 %             3 bytes_per_second=4.96% items_per_second=4.96%
DuplicateFinder/1000_files_256KiB/threads_2/1000/262144/2_mean          437 ms         26.5 ms            3 bytes_per_second=9.22621G/s items_per_second=37.7905k/s
DuplicateFinder/1000_files_256KiB/threads_2/1000/262144/2_median        437 ms         26.5 ms            3 bytes_per_second=9.22869G/s items_per_second=37.8007k/s
DuplicateFinder/1000_files_256KiB/threads_2/1000/262144/2_stddev       1.36 ms        0.229 ms            3 bytes_per_second=81.6333M/s items_per_second=326.533/s
DuplicateFinder/1000_files_256KiB/threads_2/1000/262144/2_cv           0.31 %          0.86 %             3 bytes_per_second=0.86% items_per_second=0.86%
DuplicateFinder/1000_files_256KiB/threads_4/1000/262144/4_mean          227 ms         22.6 ms            3 bytes_per_second=10.8199G/s items_per_second=44.3183k/s
DuplicateFinder/1000_files_256KiB/threads_4/1000/262144/4_median        227 ms         22.3 ms            3 bytes_per_second=10.9304G/s items_per_second=44.771k/s
DuplicateFinder/1000_files_256KiB/threads_4/1000/262144/4_stddev      0.744 ms         1.10 ms            3 bytes_per_second=530.304M/s items_per_second=2.12121k/s
DuplicateFinder/1000_files_256KiB/threads_4/1000/262144/4_cv           0.33 %          4.86 %             3 bytes_per_second=4.79% items_per_second=4.79%
DuplicateFinder/1000_files_256KiB/threads_8/1000/262144/8_mean          123 ms         22.2 ms            3 bytes_per_second=11.0126G/s items_per_second=45.1075k/s
DuplicateFinder/1000_files_256KiB/threads_8/1000/262144/8_median        123 ms         22.0 ms            3 bytes_per_second=11.0912G/s items_per_second=45.4294k/s
DuplicateFinder/1000_files_256KiB/threads_8/1000/262144/8_stddev      0.665 ms         1.12 ms            3 bytes_per_second=559.751M/s items_per_second=2.239k/s
DuplicateFinder/1000_files_256KiB/threads_8/1000/262144/8_cv           0.54 %          5.02 %             3 bytes_per_second=4.96% items_per_second=4.96%
wxm@wxm-Precision-7920-Tower:~/FileSystemTools$ 
```

## V2 (内存清零)：优化了  Hasher  内部  std::vector  的清零操作

Commit Hash：d39a756702fe1917ab851c1945cdbb8af41dcac0

```
wxm@wxm-Precision-7920-Tower:~/FileSystemTools$ ./build-bench/benchmark/dedup/dedupBenchmark \
  --benchmark_min_time=1.0 \
  --benchmark_repetitions=3 \
  --benchmark_report_aggregates_only=true
2026-06-25T10:07:42+08:00
Running ./build-bench/benchmark/dedup/dedupBenchmark
Run on (24 X 3500 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x12)
  L1 Instruction 32 KiB (x12)
  L2 Unified 1024 KiB (x12)
  L3 Unified 16896 KiB (x1)
Load Average: 4.36, 2.19, 1.40
***WARNING*** CPU scaling is enabled, the benchmark real time measurements may be noisy and will incur extra overhead.
---------------------------------------------------------------------------------------------------------------------------------------------------------
Benchmark                                                                                               Time             CPU   Iterations UserCounters...
---------------------------------------------------------------------------------------------------------------------------------------------------------
DuplicateFinder/100_files_4KiB/threads_1/100/4096/1/20/0/0_mean                                      2.40 ms         1.30 ms            3 bytes_per_second=300.636M/s items_per_second=76.9629k/s
DuplicateFinder/100_files_4KiB/threads_1/100/4096/1/20/0/0_median                                    2.43 ms         1.31 ms            3 bytes_per_second=297.154M/s items_per_second=76.0714k/s
DuplicateFinder/100_files_4KiB/threads_1/100/4096/1/20/0/0_stddev                                   0.156 ms        0.084 ms            3 bytes_per_second=19.7866M/s items_per_second=5.06537k/s
DuplicateFinder/100_files_4KiB/threads_1/100/4096/1/20/0/0_cv                                        6.48 %          6.48 %             3 bytes_per_second=6.58% items_per_second=6.58%
DuplicateFinder/100_files_4KiB/threads_2/100/4096/2/20/0/0_mean                                      1.40 ms        0.823 ms            3 bytes_per_second=474.728M/s items_per_second=121.53k/s
DuplicateFinder/100_files_4KiB/threads_2/100/4096/2/20/0/0_median                                    1.40 ms        0.822 ms            3 bytes_per_second=475.407M/s items_per_second=121.704k/s
DuplicateFinder/100_files_4KiB/threads_2/100/4096/2/20/0/0_stddev                                   0.007 ms        0.005 ms            3 bytes_per_second=2.84257M/s items_per_second=727.699/s
DuplicateFinder/100_files_4KiB/threads_2/100/4096/2/20/0/0_cv                                        0.54 %          0.60 %             3 bytes_per_second=0.60% items_per_second=0.60%
DuplicateFinder/100_files_4KiB/threads_4/100/4096/4/20/0/0_mean                                     0.867 ms        0.582 ms            3 bytes_per_second=675.575M/s items_per_second=172.947k/s
DuplicateFinder/100_files_4KiB/threads_4/100/4096/4/20/0/0_median                                   0.849 ms        0.549 ms            3 bytes_per_second=711.11M/s items_per_second=182.044k/s
DuplicateFinder/100_files_4KiB/threads_4/100/4096/4/20/0/0_stddev                                   0.049 ms        0.061 ms            3 bytes_per_second=66.5857M/s items_per_second=17.0459k/s
DuplicateFinder/100_files_4KiB/threads_4/100/4096/4/20/0/0_cv                                        5.67 %         10.45 %             3 bytes_per_second=9.86% items_per_second=9.86%
DuplicateFinder/100_files_4KiB/threads_8/100/4096/8/20/0/0_mean                                     0.749 ms        0.602 ms            3 bytes_per_second=651.014M/s items_per_second=166.66k/s
DuplicateFinder/100_files_4KiB/threads_8/100/4096/8/20/0/0_median                                   0.728 ms        0.584 ms            3 bytes_per_second=668.32M/s items_per_second=171.09k/s
DuplicateFinder/100_files_4KiB/threads_8/100/4096/8/20/0/0_stddev                                   0.057 ms        0.043 ms            3 bytes_per_second=44.9176M/s items_per_second=11.4989k/s
DuplicateFinder/100_files_4KiB/threads_8/100/4096/8/20/0/0_cv                                        7.55 %          7.15 %             3 bytes_per_second=6.90% items_per_second=6.90%
DuplicateFinder/1000_files_4KiB/threads_1/1000/4096/1/20/0/0_mean                                    22.2 ms         11.9 ms            3 bytes_per_second=327.627M/s items_per_second=83.8726k/s
DuplicateFinder/1000_files_4KiB/threads_1/1000/4096/1/20/0/0_median                                  22.1 ms         11.9 ms            3 bytes_per_second=328.321M/s items_per_second=84.0501k/s
DuplicateFinder/1000_files_4KiB/threads_1/1000/4096/1/20/0/0_stddev                                 0.252 ms        0.152 ms            3 bytes_per_second=4.15977M/s items_per_second=1064.9/s
DuplicateFinder/1000_files_4KiB/threads_1/1000/4096/1/20/0/0_cv                                      1.14 %          1.27 %             3 bytes_per_second=1.27% items_per_second=1.27%
DuplicateFinder/1000_files_4KiB/threads_2/1000/4096/2/20/0/0_mean                                    12.6 ms         7.47 ms            3 bytes_per_second=523.227M/s items_per_second=133.946k/s
DuplicateFinder/1000_files_4KiB/threads_2/1000/4096/2/20/0/0_median                                  12.6 ms         7.45 ms            3 bytes_per_second=524.185M/s items_per_second=134.191k/s
DuplicateFinder/1000_files_4KiB/threads_2/1000/4096/2/20/0/0_stddev                                 0.084 ms        0.059 ms            3 bytes_per_second=4.12237M/s items_per_second=1055.33/s
DuplicateFinder/1000_files_4KiB/threads_2/1000/4096/2/20/0/0_cv                                      0.67 %          0.79 %             3 bytes_per_second=0.79% items_per_second=0.79%
DuplicateFinder/1000_files_4KiB/threads_4/1000/4096/4/20/0/0_mean                                    7.92 ms         5.39 ms            3 bytes_per_second=726.548M/s items_per_second=185.996k/s
DuplicateFinder/1000_files_4KiB/threads_4/1000/4096/4/20/0/0_median                                  8.10 ms         5.45 ms            3 bytes_per_second=716.397M/s items_per_second=183.398k/s
DuplicateFinder/1000_files_4KiB/threads_4/1000/4096/4/20/0/0_stddev                                 0.327 ms        0.273 ms            3 bytes_per_second=37.4629M/s items_per_second=9.59051k/s
DuplicateFinder/1000_files_4KiB/threads_4/1000/4096/4/20/0/0_cv                                      4.13 %          5.06 %             3 bytes_per_second=5.16% items_per_second=5.16%
DuplicateFinder/1000_files_4KiB/threads_8/1000/4096/8/20/0/0_mean                                    6.83 ms         5.64 ms            3 bytes_per_second=693.06M/s items_per_second=177.423k/s
DuplicateFinder/1000_files_4KiB/threads_8/1000/4096/8/20/0/0_median                                  6.85 ms         5.68 ms            3 bytes_per_second=687.675M/s items_per_second=176.045k/s
DuplicateFinder/1000_files_4KiB/threads_8/1000/4096/8/20/0/0_stddev                                 0.066 ms        0.100 ms            3 bytes_per_second=12.3753M/s items_per_second=3.16808k/s
DuplicateFinder/1000_files_4KiB/threads_8/1000/4096/8/20/0/0_cv                                      0.96 %          1.77 %             3 bytes_per_second=1.79% items_per_second=1.79%
DuplicateFinder/10000_files_4KiB/threads_1/10000/4096/1/20/0/0_mean                                   129 ms         76.3 ms            3 bytes_per_second=511.887M/s items_per_second=131.043k/s
DuplicateFinder/10000_files_4KiB/threads_1/10000/4096/1/20/0/0_median                                 130 ms         76.0 ms            3 bytes_per_second=514.015M/s items_per_second=131.588k/s
DuplicateFinder/10000_files_4KiB/threads_1/10000/4096/1/20/0/0_stddev                                6.01 ms         1.44 ms            3 bytes_per_second=9.57341M/s items_per_second=2.45079k/s
DuplicateFinder/10000_files_4KiB/threads_1/10000/4096/1/20/0/0_cv                                    4.65 %          1.88 %             3 bytes_per_second=1.87% items_per_second=1.87%
DuplicateFinder/10000_files_4KiB/threads_2/10000/4096/2/20/0/0_mean                                   105 ms         68.9 ms            3 bytes_per_second=567.583M/s items_per_second=145.301k/s
DuplicateFinder/10000_files_4KiB/threads_2/10000/4096/2/20/0/0_median                                 109 ms         70.3 ms            3 bytes_per_second=555.802M/s items_per_second=142.285k/s
DuplicateFinder/10000_files_4KiB/threads_2/10000/4096/2/20/0/0_stddev                                8.01 ms         2.89 ms            3 bytes_per_second=24.3497M/s items_per_second=6.23352k/s
DuplicateFinder/10000_files_4KiB/threads_2/10000/4096/2/20/0/0_cv                                    7.63 %          4.19 %             3 bytes_per_second=4.29% items_per_second=4.29%
DuplicateFinder/10000_files_4KiB/threads_4/10000/4096/4/20/0/0_mean                                  83.3 ms         60.9 ms            3 bytes_per_second=642.05M/s items_per_second=164.365k/s
DuplicateFinder/10000_files_4KiB/threads_4/10000/4096/4/20/0/0_median                                82.7 ms         60.0 ms            3 bytes_per_second=651.007M/s items_per_second=166.658k/s
DuplicateFinder/10000_files_4KiB/threads_4/10000/4096/4/20/0/0_stddev                                1.53 ms         1.70 ms            3 bytes_per_second=17.6778M/s items_per_second=4.52552k/s
DuplicateFinder/10000_files_4KiB/threads_4/10000/4096/4/20/0/0_cv                                    1.84 %          2.80 %             3 bytes_per_second=2.75% items_per_second=2.75%
DuplicateFinder/10000_files_4KiB/threads_8/10000/4096/8/20/0/0_mean                                  70.2 ms         61.2 ms            3 bytes_per_second=639.346M/s items_per_second=163.673k/s
DuplicateFinder/10000_files_4KiB/threads_8/10000/4096/8/20/0/0_median                                70.9 ms         61.3 ms            3 bytes_per_second=637.353M/s items_per_second=163.162k/s
DuplicateFinder/10000_files_4KiB/threads_8/10000/4096/8/20/0/0_stddev                                1.81 ms         2.92 ms            3 bytes_per_second=30.5827M/s items_per_second=7.82918k/s
DuplicateFinder/10000_files_4KiB/threads_8/10000/4096/8/20/0/0_cv                                    2.57 %          4.77 %             3 bytes_per_second=4.78% items_per_second=4.78%
DuplicateFinder/1000_files_256KiB/threads_1/1000/262144/1/20/0/0_mean                                 169 ms         15.2 ms            3 bytes_per_second=16.1137G/s items_per_second=66.0016k/s
DuplicateFinder/1000_files_256KiB/threads_1/1000/262144/1/20/0/0_median                               168 ms         15.1 ms            3 bytes_per_second=16.2142G/s items_per_second=66.4134k/s
DuplicateFinder/1000_files_256KiB/threads_1/1000/262144/1/20/0/0_stddev                             0.813 ms        0.385 ms            3 bytes_per_second=415.727M/s items_per_second=1.66291k/s
DuplicateFinder/1000_files_256KiB/threads_1/1000/262144/1/20/0/0_cv                                  0.48 %          2.54 %             3 bytes_per_second=2.52% items_per_second=2.52%
DuplicateFinder/1000_files_256KiB/threads_2/1000/262144/2/20/0/0_mean                                91.8 ms         13.8 ms            3 bytes_per_second=17.7838G/s items_per_second=72.8424k/s
DuplicateFinder/1000_files_256KiB/threads_2/1000/262144/2/20/0/0_median                              91.5 ms         13.4 ms            3 bytes_per_second=18.2684G/s items_per_second=74.8274k/s
DuplicateFinder/1000_files_256KiB/threads_2/1000/262144/2/20/0/0_stddev                             0.751 ms        0.887 ms            3 bytes_per_second=1.10811G/s items_per_second=4.53883k/s
DuplicateFinder/1000_files_256KiB/threads_2/1000/262144/2/20/0/0_cv                                  0.82 %          6.45 %             3 bytes_per_second=6.23% items_per_second=6.23%
DuplicateFinder/1000_files_256KiB/threads_4/1000/262144/4/20/0/0_mean                                49.8 ms         10.5 ms            3 bytes_per_second=24.7749G/s items_per_second=101.478k/s
DuplicateFinder/1000_files_256KiB/threads_4/1000/262144/4/20/0/0_median                              51.7 ms         12.1 ms            3 bytes_per_second=20.2257G/s items_per_second=82.8445k/s
DuplicateFinder/1000_files_256KiB/threads_4/1000/262144/4/20/0/0_stddev                              3.44 ms         2.81 ms            3 bytes_per_second=7.89107G/s items_per_second=32.3218k/s
DuplicateFinder/1000_files_256KiB/threads_4/1000/262144/4/20/0/0_cv                                  6.90 %         26.90 %             3 bytes_per_second=31.85% items_per_second=31.85%
DuplicateFinder/1000_files_256KiB/threads_8/1000/262144/8/20/0/0_mean                                32.1 ms         10.5 ms            3 bytes_per_second=23.6301G/s items_per_second=96.7888k/s
DuplicateFinder/1000_files_256KiB/threads_8/1000/262144/8/20/0/0_median                              31.4 ms         9.99 ms            3 bytes_per_second=24.4401G/s items_per_second=100.107k/s
DuplicateFinder/1000_files_256KiB/threads_8/1000/262144/8/20/0/0_stddev                              2.68 ms         1.79 ms            3 bytes_per_second=3.78641G/s items_per_second=15.5091k/s
DuplicateFinder/1000_files_256KiB/threads_8/1000/262144/8/20/0/0_cv                                  8.34 %         17.00 %             3 bytes_per_second=16.02% items_per_second=16.02%
DuplicateFinder/many_small_unique_same_size/threads_1/10000/4096/1/0/1/0_mean                         332 ms          158 ms            3 bytes_per_second=247.304M/s items_per_second=63.3097k/s
DuplicateFinder/many_small_unique_same_size/threads_1/10000/4096/1/0/1/0_median                       332 ms          158 ms            3 bytes_per_second=247.626M/s items_per_second=63.3923k/s
DuplicateFinder/many_small_unique_same_size/threads_1/10000/4096/1/0/1/0_stddev                     0.923 ms         4.13 ms            3 bytes_per_second=6.45377M/s items_per_second=1.65217k/s
DuplicateFinder/many_small_unique_same_size/threads_1/10000/4096/1/0/1/0_cv                          0.28 %          2.62 %             3 bytes_per_second=2.61% items_per_second=2.61%
DuplicateFinder/many_small_unique_same_size/threads_2/10000/4096/2/0/1/0_mean                         220 ms          125 ms            3 bytes_per_second=312.18M/s items_per_second=79.918k/s
DuplicateFinder/many_small_unique_same_size/threads_2/10000/4096/2/0/1/0_median                       219 ms          125 ms            3 bytes_per_second=311.916M/s items_per_second=79.8504k/s
DuplicateFinder/many_small_unique_same_size/threads_2/10000/4096/2/0/1/0_stddev                      2.38 ms        0.461 ms            3 bytes_per_second=1.15103M/s items_per_second=294.664/s
DuplicateFinder/many_small_unique_same_size/threads_2/10000/4096/2/0/1/0_cv                          1.08 %          0.37 %             3 bytes_per_second=0.37% items_per_second=0.37%
DuplicateFinder/many_small_unique_same_size/threads_4/10000/4096/4/0/1/0_mean                         147 ms         99.3 ms            3 bytes_per_second=393.751M/s items_per_second=100.8k/s
DuplicateFinder/many_small_unique_same_size/threads_4/10000/4096/4/0/1/0_median                       147 ms          100 ms            3 bytes_per_second=388.909M/s items_per_second=99.5608k/s
DuplicateFinder/many_small_unique_same_size/threads_4/10000/4096/4/0/1/0_stddev                      3.05 ms         3.15 ms            3 bytes_per_second=12.7041M/s items_per_second=3.25224k/s
DuplicateFinder/many_small_unique_same_size/threads_4/10000/4096/4/0/1/0_cv                          2.08 %          3.18 %             3 bytes_per_second=3.23% items_per_second=3.23%
DuplicateFinder/many_small_unique_same_size/threads_8/10000/4096/8/0/1/0_mean                         117 ms         95.4 ms            3 bytes_per_second=409.442M/s items_per_second=104.817k/s
DuplicateFinder/many_small_unique_same_size/threads_8/10000/4096/8/0/1/0_median                       117 ms         95.4 ms            3 bytes_per_second=409.539M/s items_per_second=104.842k/s
DuplicateFinder/many_small_unique_same_size/threads_8/10000/4096/8/0/1/0_stddev                      3.63 ms        0.930 ms            3 bytes_per_second=3.99111M/s items_per_second=1021.72/s
DuplicateFinder/many_small_unique_same_size/threads_8/10000/4096/8/0/1/0_cv                          3.10 %          0.98 %             3 bytes_per_second=0.97% items_per_second=0.97%
DuplicateFinder/many_small_duplicates/threads_1/10000/4096/1/100/1/0_mean                             334 ms          161 ms            3 bytes_per_second=242.36M/s items_per_second=62.0441k/s
DuplicateFinder/many_small_duplicates/threads_1/10000/4096/1/100/1/0_median                           335 ms          163 ms            3 bytes_per_second=239.669M/s items_per_second=61.3553k/s
DuplicateFinder/many_small_duplicates/threads_1/10000/4096/1/100/1/0_stddev                          4.39 ms         3.51 ms            3 bytes_per_second=5.33746M/s items_per_second=1.36639k/s
DuplicateFinder/many_small_duplicates/threads_1/10000/4096/1/100/1/0_cv                              1.32 %          2.18 %             3 bytes_per_second=2.20% items_per_second=2.20%
DuplicateFinder/many_small_duplicates/threads_2/10000/4096/2/100/1/0_mean                             224 ms          125 ms            3 bytes_per_second=313.047M/s items_per_second=80.1399k/s
DuplicateFinder/many_small_duplicates/threads_2/10000/4096/2/100/1/0_median                           224 ms          125 ms            3 bytes_per_second=313.11M/s items_per_second=80.1563k/s
DuplicateFinder/many_small_duplicates/threads_2/10000/4096/2/100/1/0_stddev                          2.44 ms         1.88 ms            3 bytes_per_second=4.70979M/s items_per_second=1.20571k/s
DuplicateFinder/many_small_duplicates/threads_2/10000/4096/2/100/1/0_cv                              1.09 %          1.51 %             3 bytes_per_second=1.50% items_per_second=1.50%
DuplicateFinder/many_small_duplicates/threads_4/10000/4096/4/100/1/0_mean                             153 ms          107 ms            3 bytes_per_second=365.411M/s items_per_second=93.5452k/s
DuplicateFinder/many_small_duplicates/threads_4/10000/4096/4/100/1/0_median                           152 ms          106 ms            3 bytes_per_second=366.954M/s items_per_second=93.9402k/s
DuplicateFinder/many_small_duplicates/threads_4/10000/4096/4/100/1/0_stddev                          2.84 ms         1.52 ms            3 bytes_per_second=5.17274M/s items_per_second=1.32422k/s
DuplicateFinder/many_small_duplicates/threads_4/10000/4096/4/100/1/0_cv                              1.86 %          1.42 %             3 bytes_per_second=1.42% items_per_second=1.42%
DuplicateFinder/many_small_duplicates/threads_8/10000/4096/8/100/1/0_mean                             121 ms         99.1 ms            3 bytes_per_second=394.432M/s items_per_second=100.975k/s
DuplicateFinder/many_small_duplicates/threads_8/10000/4096/8/100/1/0_median                           121 ms          100 ms            3 bytes_per_second=390.356M/s items_per_second=99.9313k/s
DuplicateFinder/many_small_duplicates/threads_8/10000/4096/8/100/1/0_stddev                          2.15 ms         3.03 ms            3 bytes_per_second=12.2275M/s items_per_second=3.13024k/s
DuplicateFinder/many_small_duplicates/threads_8/10000/4096/8/100/1/0_cv                              1.78 %          3.06 %             3 bytes_per_second=3.10% items_per_second=3.10%
DuplicateFinder/large_same_size_different_content/threads_1/20/8388608/1/0/1/0_mean                   479 ms         1.45 ms            3 bytes_per_second=109.885G/s items_per_second=14.0653k/s
DuplicateFinder/large_same_size_different_content/threads_1/20/8388608/1/0/1/0_median                 479 ms         1.57 ms            3 bytes_per_second=99.3317G/s items_per_second=12.7145k/s
DuplicateFinder/large_same_size_different_content/threads_1/20/8388608/1/0/1/0_stddev                1.23 ms        0.229 ms            3 bytes_per_second=19.1194G/s items_per_second=2.44728k/s
DuplicateFinder/large_same_size_different_content/threads_1/20/8388608/1/0/1/0_cv                    0.26 %         15.82 %             3 bytes_per_second=17.40% items_per_second=17.40%
DuplicateFinder/large_same_size_different_content/threads_2/20/8388608/2/0/1/0_mean                   243 ms         1.13 ms            3 bytes_per_second=139.002G/s items_per_second=17.7922k/s
DuplicateFinder/large_same_size_different_content/threads_2/20/8388608/2/0/1/0_median                 243 ms         1.18 ms            3 bytes_per_second=132.198G/s items_per_second=16.9213k/s
DuplicateFinder/large_same_size_different_content/threads_2/20/8388608/2/0/1/0_stddev               0.062 ms        0.130 ms            3 bytes_per_second=16.8474G/s items_per_second=2.15647k/s
DuplicateFinder/large_same_size_different_content/threads_2/20/8388608/2/0/1/0_cv                    0.03 %         11.44 %             3 bytes_per_second=12.12% items_per_second=12.12%
DuplicateFinder/large_same_size_different_content/threads_4/20/8388608/4/0/1/0_mean                   124 ms        0.835 ms            3 bytes_per_second=192.655G/s items_per_second=24.6599k/s
DuplicateFinder/large_same_size_different_content/threads_4/20/8388608/4/0/1/0_median                 124 ms        0.927 ms            3 bytes_per_second=168.526G/s items_per_second=21.5713k/s
DuplicateFinder/large_same_size_different_content/threads_4/20/8388608/4/0/1/0_stddev               0.573 ms        0.164 ms            3 bytes_per_second=42.724G/s items_per_second=5.46868k/s
DuplicateFinder/large_same_size_different_content/threads_4/20/8388608/4/0/1/0_cv                    0.46 %         19.66 %             3 bytes_per_second=22.18% items_per_second=22.18%
DuplicateFinder/large_same_size_different_content/threads_8/20/8388608/8/0/1/0_mean                  76.9 ms        0.719 ms            3 bytes_per_second=220.501G/s items_per_second=28.2242k/s
DuplicateFinder/large_same_size_different_content/threads_8/20/8388608/8/0/1/0_median                76.8 ms        0.687 ms            3 bytes_per_second=227.544G/s items_per_second=29.1257k/s
DuplicateFinder/large_same_size_different_content/threads_8/20/8388608/8/0/1/0_stddev               0.099 ms        0.108 ms            3 bytes_per_second=31.3747G/s items_per_second=4.01596k/s
DuplicateFinder/large_same_size_different_content/threads_8/20/8388608/8/0/1/0_cv                    0.13 %         15.01 %             3 bytes_per_second=14.23% items_per_second=14.23%
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_1/20/8388608/1/0/1/1_mean          481 ms         1.53 ms            3 bytes_per_second=102.28G/s items_per_second=13.0919k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_1/20/8388608/1/0/1/1_median        481 ms         1.52 ms            3 bytes_per_second=102.808G/s items_per_second=13.1594k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_1/20/8388608/1/0/1/1_stddev      0.395 ms        0.063 ms            3 bytes_per_second=4.20161G/s items_per_second=537.807/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_1/20/8388608/1/0/1/1_cv           0.08 %          4.14 %             3 bytes_per_second=4.11% items_per_second=4.11%
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_2/20/8388608/2/0/1/1_mean          243 ms         1.12 ms            3 bytes_per_second=140.357G/s items_per_second=17.9657k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_2/20/8388608/2/0/1/1_median        243 ms         1.15 ms            3 bytes_per_second=135.724G/s items_per_second=17.3727k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_2/20/8388608/2/0/1/1_stddev      0.227 ms        0.087 ms            3 bytes_per_second=11.3513G/s items_per_second=1.45297k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_2/20/8388608/2/0/1/1_cv           0.09 %          7.77 %             3 bytes_per_second=8.09% items_per_second=8.09%
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_4/20/8388608/4/0/1/1_mean          124 ms        0.887 ms            3 bytes_per_second=176.442G/s items_per_second=22.5846k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_4/20/8388608/4/0/1/1_median        123 ms        0.908 ms            3 bytes_per_second=172.17G/s items_per_second=22.0378k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_4/20/8388608/4/0/1/1_stddev      0.794 ms        0.037 ms            3 bytes_per_second=7.50919G/s items_per_second=961.176/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_4/20/8388608/4/0/1/1_cv           0.64 %          4.15 %             3 bytes_per_second=4.26% items_per_second=4.26%
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_8/20/8388608/8/0/1/1_mean         77.1 ms        0.798 ms            3 bytes_per_second=197.382G/s items_per_second=25.2649k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_8/20/8388608/8/0/1/1_median       76.9 ms        0.785 ms            3 bytes_per_second=199.105G/s items_per_second=25.4854k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_8/20/8388608/8/0/1/1_stddev      0.388 ms        0.091 ms            3 bytes_per_second=22.0162G/s items_per_second=2.81808k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_8/20/8388608/8/0/1/1_cv           0.50 %         11.37 %             3 bytes_per_second=11.15% items_per_second=11.15%
DuplicateFinder/large_duplicates/threads_1/20/8388608/1/100/1/0_mean                                  482 ms         1.58 ms            3 bytes_per_second=98.843G/s items_per_second=12.6519k/s
DuplicateFinder/large_duplicates/threads_1/20/8388608/1/100/1/0_median                                482 ms         1.61 ms            3 bytes_per_second=97.0138G/s items_per_second=12.4178k/s
DuplicateFinder/large_duplicates/threads_1/20/8388608/1/100/1/0_stddev                              0.965 ms        0.096 ms            3 bytes_per_second=6.10236G/s items_per_second=781.103/s
DuplicateFinder/large_duplicates/threads_1/20/8388608/1/100/1/0_cv                                   0.20 %          6.03 %             3 bytes_per_second=6.17% items_per_second=6.17%
DuplicateFinder/large_duplicates/threads_2/20/8388608/2/100/1/0_mean                                  244 ms         1.25 ms            3 bytes_per_second=125.307G/s items_per_second=16.0393k/s
DuplicateFinder/large_duplicates/threads_2/20/8388608/2/100/1/0_median                                243 ms         1.27 ms            3 bytes_per_second=123.36G/s items_per_second=15.7901k/s
DuplicateFinder/large_duplicates/threads_2/20/8388608/2/100/1/0_stddev                              0.880 ms        0.035 ms            3 bytes_per_second=3.53504G/s items_per_second=452.485/s
DuplicateFinder/large_duplicates/threads_2/20/8388608/2/100/1/0_cv                                   0.36 %          2.78 %             3 bytes_per_second=2.82% items_per_second=2.82%
DuplicateFinder/large_duplicates/threads_4/20/8388608/4/100/1/0_mean                                  124 ms        0.959 ms            3 bytes_per_second=163.102G/s items_per_second=20.8771k/s
DuplicateFinder/large_duplicates/threads_4/20/8388608/4/100/1/0_median                                124 ms        0.966 ms            3 bytes_per_second=161.704G/s items_per_second=20.6981k/s
DuplicateFinder/large_duplicates/threads_4/20/8388608/4/100/1/0_stddev                              0.433 ms        0.036 ms            3 bytes_per_second=6.11448G/s items_per_second=782.653/s
DuplicateFinder/large_duplicates/threads_4/20/8388608/4/100/1/0_cv                                   0.35 %          3.71 %             3 bytes_per_second=3.75% items_per_second=3.75%
DuplicateFinder/large_duplicates/threads_8/20/8388608/8/100/1/0_mean                                 77.4 ms        0.691 ms            3 bytes_per_second=229.71G/s items_per_second=29.4029k/s
DuplicateFinder/large_duplicates/threads_8/20/8388608/8/100/1/0_median                               77.4 ms        0.659 ms            3 bytes_per_second=237.088G/s items_per_second=30.3473k/s
DuplicateFinder/large_duplicates/threads_8/20/8388608/8/100/1/0_stddev                              0.301 ms        0.110 ms            3 bytes_per_second=34.5304G/s items_per_second=4.41989k/s
DuplicateFinder/large_duplicates/threads_8/20/8388608/8/100/1/0_cv                                   0.39 %         15.88 %             3 bytes_per_second=15.03% items_per_second=15.03%
DuplicateFinder/mixed_realistic/threads_1/1000/-1/1/20/0/0_mean                                      3319 ms         40.1 ms            3 items_per_second=24.9789k/s
DuplicateFinder/mixed_realistic/threads_1/1000/-1/1/20/0/0_median                                    3320 ms         39.5 ms            3 items_per_second=25.3038k/s
DuplicateFinder/mixed_realistic/threads_1/1000/-1/1/20/0/0_stddev                                    5.81 ms         1.09 ms            3 items_per_second=670.723/s
DuplicateFinder/mixed_realistic/threads_1/1000/-1/1/20/0/0_cv                                        0.18 %          2.73 %             3 items_per_second=2.69%
DuplicateFinder/mixed_realistic/threads_2/1000/-1/2/20/0/0_mean                                      1680 ms         26.2 ms            3 items_per_second=38.4524k/s
DuplicateFinder/mixed_realistic/threads_2/1000/-1/2/20/0/0_median                                    1680 ms         27.1 ms            3 items_per_second=36.8568k/s
DuplicateFinder/mixed_realistic/threads_2/1000/-1/2/20/0/0_stddev                                    3.74 ms         2.97 ms            3 items_per_second=4.58334k/s
DuplicateFinder/mixed_realistic/threads_2/1000/-1/2/20/0/0_cv                                        0.22 %         11.33 %             3 items_per_second=11.92%
DuplicateFinder/mixed_realistic/threads_4/1000/-1/4/20/0/0_mean                                       862 ms         25.6 ms            3 items_per_second=39.1116k/s
DuplicateFinder/mixed_realistic/threads_4/1000/-1/4/20/0/0_median                                     862 ms         26.1 ms            3 items_per_second=38.2927k/s
DuplicateFinder/mixed_realistic/threads_4/1000/-1/4/20/0/0_stddev                                    4.17 ms         1.13 ms            3 items_per_second=1.76164k/s
DuplicateFinder/mixed_realistic/threads_4/1000/-1/4/20/0/0_cv                                        0.48 %          4.40 %             3 items_per_second=4.50%
DuplicateFinder/mixed_realistic/threads_8/1000/-1/8/20/0/0_mean                                       461 ms         17.4 ms            3 items_per_second=57.9237k/s
DuplicateFinder/mixed_realistic/threads_8/1000/-1/8/20/0/0_median                                     462 ms         17.6 ms            3 items_per_second=56.8356k/s
DuplicateFinder/mixed_realistic/threads_8/1000/-1/8/20/0/0_stddev                                    2.11 ms         1.57 ms            3 items_per_second=5.36472k/s
DuplicateFinder/mixed_realistic/threads_8/1000/-1/8/20/0/0_cv                                        0.46 %          9.05 %             3 items_per_second=9.26%
wxm@wxm-Precision-7920-Tower:~/FileSystemTools$
```

##  V3 (小文件批量切片)

Commit Hash：b4fc4af268eef56030fe0845a1df7016aeb88648

```
wxm@wxm-Precision-7920-Tower:~/FileSystemTools$ ./build-bench/benchmark/dedup/dedupBenchmark \
  --benchmark_min_time=1.0 \
  --benchmark_repetitions=3 \
  --benchmark_report_aggregates_only=true
2026-06-25T10:23:30+08:00
Running ./build-bench/benchmark/dedup/dedupBenchmark
Run on (24 X 3500 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x12)
  L1 Instruction 32 KiB (x12)
  L2 Unified 1024 KiB (x12)
  L3 Unified 16896 KiB (x1)
Load Average: 1.55, 3.09, 2.70
***WARNING*** CPU scaling is enabled, the benchmark real time measurements may be noisy and will incur extra overhead.
---------------------------------------------------------------------------------------------------------------------------------------------------------
Benchmark                                                                                               Time             CPU   Iterations UserCounters...
---------------------------------------------------------------------------------------------------------------------------------------------------------
DuplicateFinder/100_files_4KiB/threads_1/100/4096/1/20/0/0_mean                                      2.77 ms         1.43 ms            3 bytes_per_second=272.558M/s items_per_second=69.7748k/s
DuplicateFinder/100_files_4KiB/threads_1/100/4096/1/20/0/0_median                                    2.77 ms         1.43 ms            3 bytes_per_second=272.431M/s items_per_second=69.7424k/s
DuplicateFinder/100_files_4KiB/threads_1/100/4096/1/20/0/0_stddev                                   0.041 ms        0.014 ms            3 bytes_per_second=2.69542M/s items_per_second=690.027/s
DuplicateFinder/100_files_4KiB/threads_1/100/4096/1/20/0/0_cv                                        1.48 %          0.99 %             3 bytes_per_second=0.99% items_per_second=0.99%
DuplicateFinder/100_files_4KiB/threads_2/100/4096/2/20/0/0_mean                                      2.79 ms         1.42 ms            3 bytes_per_second=274.496M/s items_per_second=70.2711k/s
DuplicateFinder/100_files_4KiB/threads_2/100/4096/2/20/0/0_median                                    2.81 ms         1.44 ms            3 bytes_per_second=272.204M/s items_per_second=69.6841k/s
DuplicateFinder/100_files_4KiB/threads_2/100/4096/2/20/0/0_stddev                                   0.052 ms        0.026 ms            3 bytes_per_second=5.09168M/s items_per_second=1.30347k/s
DuplicateFinder/100_files_4KiB/threads_2/100/4096/2/20/0/0_cv                                        1.86 %          1.84 %             3 bytes_per_second=1.85% items_per_second=1.85%
DuplicateFinder/100_files_4KiB/threads_4/100/4096/4/20/0/0_mean                                      2.84 ms         1.45 ms            3 bytes_per_second=270.089M/s items_per_second=69.1428k/s
DuplicateFinder/100_files_4KiB/threads_4/100/4096/4/20/0/0_median                                    2.84 ms         1.45 ms            3 bytes_per_second=269.192M/s items_per_second=68.9132k/s
DuplicateFinder/100_files_4KiB/threads_4/100/4096/4/20/0/0_stddev                                   0.039 ms        0.021 ms            3 bytes_per_second=3.94633M/s items_per_second=1010.26/s
DuplicateFinder/100_files_4KiB/threads_4/100/4096/4/20/0/0_cv                                        1.36 %          1.45 %             3 bytes_per_second=1.46% items_per_second=1.46%
DuplicateFinder/100_files_4KiB/threads_8/100/4096/8/20/0/0_mean                                      2.76 ms         1.40 ms            3 bytes_per_second=279.278M/s items_per_second=71.4951k/s
DuplicateFinder/100_files_4KiB/threads_8/100/4096/8/20/0/0_median                                    2.74 ms         1.38 ms            3 bytes_per_second=284.054M/s items_per_second=72.7178k/s
DuplicateFinder/100_files_4KiB/threads_8/100/4096/8/20/0/0_stddev                                   0.101 ms        0.063 ms            3 bytes_per_second=12.1904M/s items_per_second=3.12075k/s
DuplicateFinder/100_files_4KiB/threads_8/100/4096/8/20/0/0_cv                                        3.67 %          4.46 %             3 bytes_per_second=4.36% items_per_second=4.36%
DuplicateFinder/1000_files_4KiB/threads_1/1000/4096/1/20/0/0_mean                                    19.2 ms         10.3 ms            3 bytes_per_second=429.041M/s items_per_second=109.834k/s
DuplicateFinder/1000_files_4KiB/threads_1/1000/4096/1/20/0/0_median                                  23.3 ms         12.4 ms            3 bytes_per_second=314.85M/s items_per_second=80.6017k/s
DuplicateFinder/1000_files_4KiB/threads_1/1000/4096/1/20/0/0_stddev                                  7.00 ms         3.76 ms            3 bytes_per_second=199.146M/s items_per_second=50.9813k/s
DuplicateFinder/1000_files_4KiB/threads_1/1000/4096/1/20/0/0_cv                                     36.37 %         36.61 %             3 bytes_per_second=46.42% items_per_second=46.42%
DuplicateFinder/1000_files_4KiB/threads_2/1000/4096/2/20/0/0_mean                                    19.0 ms         11.9 ms            3 bytes_per_second=329.255M/s items_per_second=84.2893k/s
DuplicateFinder/1000_files_4KiB/threads_2/1000/4096/2/20/0/0_median                                  19.0 ms         11.9 ms            3 bytes_per_second=328.912M/s items_per_second=84.2016k/s
DuplicateFinder/1000_files_4KiB/threads_2/1000/4096/2/20/0/0_stddev                                 0.139 ms        0.176 ms            3 bytes_per_second=4.89969M/s items_per_second=1.25432k/s
DuplicateFinder/1000_files_4KiB/threads_2/1000/4096/2/20/0/0_cv                                      0.73 %          1.49 %             3 bytes_per_second=1.49% items_per_second=1.49%
DuplicateFinder/1000_files_4KiB/threads_4/1000/4096/4/20/0/0_mean                                    11.2 ms         7.13 ms            3 bytes_per_second=548.112M/s items_per_second=140.317k/s
DuplicateFinder/1000_files_4KiB/threads_4/1000/4096/4/20/0/0_median                                  11.3 ms         7.28 ms            3 bytes_per_second=536.664M/s items_per_second=137.386k/s
DuplicateFinder/1000_files_4KiB/threads_4/1000/4096/4/20/0/0_stddev                                 0.294 ms        0.266 ms            3 bytes_per_second=20.9028M/s items_per_second=5.3511k/s
DuplicateFinder/1000_files_4KiB/threads_4/1000/4096/4/20/0/0_cv                                      2.63 %          3.73 %             3 bytes_per_second=3.81% items_per_second=3.81%
DuplicateFinder/1000_files_4KiB/threads_8/1000/4096/8/20/0/0_mean                                    11.1 ms         7.15 ms            3 bytes_per_second=546.902M/s items_per_second=140.007k/s
DuplicateFinder/1000_files_4KiB/threads_8/1000/4096/8/20/0/0_median                                  11.2 ms         7.13 ms            3 bytes_per_second=547.759M/s items_per_second=140.226k/s
DuplicateFinder/1000_files_4KiB/threads_8/1000/4096/8/20/0/0_stddev                                 0.312 ms        0.299 ms            3 bytes_per_second=22.791M/s items_per_second=5.83451k/s
DuplicateFinder/1000_files_4KiB/threads_8/1000/4096/8/20/0/0_cv                                      2.80 %          4.18 %             3 bytes_per_second=4.17% items_per_second=4.17%
DuplicateFinder/10000_files_4KiB/threads_1/10000/4096/1/20/0/0_mean                                   139 ms         74.5 ms            3 bytes_per_second=524.907M/s items_per_second=134.376k/s
DuplicateFinder/10000_files_4KiB/threads_1/10000/4096/1/20/0/0_median                                 138 ms         73.6 ms            3 bytes_per_second=530.485M/s items_per_second=135.804k/s
DuplicateFinder/10000_files_4KiB/threads_1/10000/4096/1/20/0/0_stddev                                3.25 ms         2.11 ms            3 bytes_per_second=14.6653M/s items_per_second=3.75432k/s
DuplicateFinder/10000_files_4KiB/threads_1/10000/4096/1/20/0/0_cv                                    2.34 %          2.83 %             3 bytes_per_second=2.79% items_per_second=2.79%
DuplicateFinder/10000_files_4KiB/threads_2/10000/4096/2/20/0/0_mean                                   111 ms         66.6 ms            3 bytes_per_second=586.61M/s items_per_second=150.172k/s
DuplicateFinder/10000_files_4KiB/threads_2/10000/4096/2/20/0/0_median                                 111 ms         66.5 ms            3 bytes_per_second=587.743M/s items_per_second=150.462k/s
DuplicateFinder/10000_files_4KiB/threads_2/10000/4096/2/20/0/0_stddev                                1.58 ms        0.791 ms            3 bytes_per_second=6.94542M/s items_per_second=1.77803k/s
DuplicateFinder/10000_files_4KiB/threads_2/10000/4096/2/20/0/0_cv                                    1.42 %          1.19 %             3 bytes_per_second=1.18% items_per_second=1.18%
DuplicateFinder/10000_files_4KiB/threads_4/10000/4096/4/20/0/0_mean                                  83.5 ms         57.7 ms            3 bytes_per_second=676.679M/s items_per_second=173.23k/s
DuplicateFinder/10000_files_4KiB/threads_4/10000/4096/4/20/0/0_median                                84.8 ms         58.0 ms            3 bytes_per_second=673.607M/s items_per_second=172.443k/s
DuplicateFinder/10000_files_4KiB/threads_4/10000/4096/4/20/0/0_stddev                                2.58 ms         1.18 ms            3 bytes_per_second=13.9009M/s items_per_second=3.55864k/s
DuplicateFinder/10000_files_4KiB/threads_4/10000/4096/4/20/0/0_cv                                    3.09 %          2.04 %             3 bytes_per_second=2.05% items_per_second=2.05%
DuplicateFinder/10000_files_4KiB/threads_8/10000/4096/8/20/0/0_mean                                  70.1 ms         55.0 ms            3 bytes_per_second=710.393M/s items_per_second=181.861k/s
DuplicateFinder/10000_files_4KiB/threads_8/10000/4096/8/20/0/0_median                                70.0 ms         54.7 ms            3 bytes_per_second=713.916M/s items_per_second=182.763k/s
DuplicateFinder/10000_files_4KiB/threads_8/10000/4096/8/20/0/0_stddev                                1.16 ms         1.24 ms            3 bytes_per_second=15.9149M/s items_per_second=4.0742k/s
DuplicateFinder/10000_files_4KiB/threads_8/10000/4096/8/20/0/0_cv                                    1.65 %          2.26 %             3 bytes_per_second=2.24% items_per_second=2.24%
DuplicateFinder/1000_files_256KiB/threads_1/1000/262144/1/20/0/0_mean                                 170 ms         13.8 ms            3 bytes_per_second=17.743G/s items_per_second=72.6754k/s
DuplicateFinder/1000_files_256KiB/threads_1/1000/262144/1/20/0/0_median                               170 ms         13.8 ms            3 bytes_per_second=17.7459G/s items_per_second=72.6871k/s
DuplicateFinder/1000_files_256KiB/threads_1/1000/262144/1/20/0/0_stddev                             0.263 ms        0.006 ms            3 bytes_per_second=8.08756M/s items_per_second=32.3503/s
DuplicateFinder/1000_files_256KiB/threads_1/1000/262144/1/20/0/0_cv                                  0.15 %          0.04 %             3 bytes_per_second=0.04% items_per_second=0.04%
DuplicateFinder/1000_files_256KiB/threads_2/1000/262144/2/20/0/0_mean                                 115 ms         13.3 ms            3 bytes_per_second=18.4224G/s items_per_second=75.4581k/s
DuplicateFinder/1000_files_256KiB/threads_2/1000/262144/2/20/0/0_median                               115 ms         13.5 ms            3 bytes_per_second=18.1203G/s items_per_second=74.2208k/s
DuplicateFinder/1000_files_256KiB/threads_2/1000/262144/2/20/0/0_stddev                             0.780 ms        0.447 ms            3 bytes_per_second=647.798M/s items_per_second=2.59119k/s
DuplicateFinder/1000_files_256KiB/threads_2/1000/262144/2/20/0/0_cv                                  0.68 %          3.37 %             3 bytes_per_second=3.43% items_per_second=3.43%
DuplicateFinder/1000_files_256KiB/threads_4/1000/262144/4/20/0/0_mean                                94.5 ms         13.1 ms            3 bytes_per_second=18.6357G/s items_per_second=76.3317k/s
DuplicateFinder/1000_files_256KiB/threads_4/1000/262144/4/20/0/0_median                              94.5 ms         13.1 ms            3 bytes_per_second=18.6173G/s items_per_second=76.2566k/s
DuplicateFinder/1000_files_256KiB/threads_4/1000/262144/4/20/0/0_stddev                              1.11 ms        0.059 ms            3 bytes_per_second=86.457M/s items_per_second=345.828/s
DuplicateFinder/1000_files_256KiB/threads_4/1000/262144/4/20/0/0_cv                                  1.17 %          0.45 %             3 bytes_per_second=0.45% items_per_second=0.45%
DuplicateFinder/1000_files_256KiB/threads_8/1000/262144/8/20/0/0_mean                                95.2 ms         12.8 ms            3 bytes_per_second=19.1442G/s items_per_second=78.4146k/s
DuplicateFinder/1000_files_256KiB/threads_8/1000/262144/8/20/0/0_median                              95.8 ms         13.3 ms            3 bytes_per_second=18.4117G/s items_per_second=75.4144k/s
DuplicateFinder/1000_files_256KiB/threads_8/1000/262144/8/20/0/0_stddev                              1.95 ms        0.909 ms            3 bytes_per_second=1.41718G/s items_per_second=5.80478k/s
DuplicateFinder/1000_files_256KiB/threads_8/1000/262144/8/20/0/0_cv                                  2.05 %          7.10 %             3 bytes_per_second=7.40% items_per_second=7.40%
DuplicateFinder/many_small_unique_same_size/threads_1/10000/4096/1/0/1/0_mean                         289 ms          100 ms            3 bytes_per_second=390.335M/s items_per_second=99.9257k/s
DuplicateFinder/many_small_unique_same_size/threads_1/10000/4096/1/0/1/0_median                       288 ms          105 ms            3 bytes_per_second=373.159M/s items_per_second=95.5286k/s
DuplicateFinder/many_small_unique_same_size/threads_1/10000/4096/1/0/1/0_stddev                      8.17 ms         7.41 ms            3 bytes_per_second=30.0646M/s items_per_second=7.69654k/s
DuplicateFinder/many_small_unique_same_size/threads_1/10000/4096/1/0/1/0_cv                          2.83 %          7.37 %             3 bytes_per_second=7.70% items_per_second=7.70%
DuplicateFinder/many_small_unique_same_size/threads_2/10000/4096/2/0/1/0_mean                         215 ms          100 ms            3 bytes_per_second=389.953M/s items_per_second=99.828k/s
DuplicateFinder/many_small_unique_same_size/threads_2/10000/4096/2/0/1/0_median                       218 ms          102 ms            3 bytes_per_second=383.441M/s items_per_second=98.161k/s
DuplicateFinder/many_small_unique_same_size/threads_2/10000/4096/2/0/1/0_stddev                      6.79 ms         2.98 ms            3 bytes_per_second=11.7932M/s items_per_second=3.01906k/s
DuplicateFinder/many_small_unique_same_size/threads_2/10000/4096/2/0/1/0_cv                          3.16 %          2.97 %             3 bytes_per_second=3.02% items_per_second=3.02%
DuplicateFinder/many_small_unique_same_size/threads_4/10000/4096/4/0/1/0_mean                         165 ms         96.7 ms            3 bytes_per_second=403.991M/s items_per_second=103.422k/s
DuplicateFinder/many_small_unique_same_size/threads_4/10000/4096/4/0/1/0_median                       166 ms         96.4 ms            3 bytes_per_second=405.031M/s items_per_second=103.688k/s
DuplicateFinder/many_small_unique_same_size/threads_4/10000/4096/4/0/1/0_stddev                      2.83 ms         1.16 ms            3 bytes_per_second=4.8116M/s items_per_second=1.23177k/s
DuplicateFinder/many_small_unique_same_size/threads_4/10000/4096/4/0/1/0_cv                          1.71 %          1.20 %             3 bytes_per_second=1.19% items_per_second=1.19%
DuplicateFinder/many_small_unique_same_size/threads_8/10000/4096/8/0/1/0_mean                         119 ms         77.1 ms            3 bytes_per_second=508.666M/s items_per_second=130.218k/s
DuplicateFinder/many_small_unique_same_size/threads_8/10000/4096/8/0/1/0_median                       123 ms         79.4 ms            3 bytes_per_second=491.899M/s items_per_second=125.926k/s
DuplicateFinder/many_small_unique_same_size/threads_8/10000/4096/8/0/1/0_stddev                      6.88 ms         6.11 ms            3 bytes_per_second=41.9207M/s items_per_second=10.7317k/s
DuplicateFinder/many_small_unique_same_size/threads_8/10000/4096/8/0/1/0_cv                          5.78 %          7.92 %             3 bytes_per_second=8.24% items_per_second=8.24%
DuplicateFinder/many_small_duplicates/threads_1/10000/4096/1/100/1/0_mean                             312 ms          110 ms            3 bytes_per_second=355.663M/s items_per_second=91.0497k/s
DuplicateFinder/many_small_duplicates/threads_1/10000/4096/1/100/1/0_median                           313 ms          110 ms            3 bytes_per_second=354.113M/s items_per_second=90.6528k/s
DuplicateFinder/many_small_duplicates/threads_1/10000/4096/1/100/1/0_stddev                          9.48 ms         2.56 ms            3 bytes_per_second=8.35023M/s items_per_second=2.13766k/s
DuplicateFinder/many_small_duplicates/threads_1/10000/4096/1/100/1/0_cv                              3.04 %          2.33 %             3 bytes_per_second=2.35% items_per_second=2.35%
DuplicateFinder/many_small_duplicates/threads_2/10000/4096/2/100/1/0_mean                             222 ms          107 ms            3 bytes_per_second=363.762M/s items_per_second=93.123k/s
DuplicateFinder/many_small_duplicates/threads_2/10000/4096/2/100/1/0_median                           222 ms          108 ms            3 bytes_per_second=361.805M/s items_per_second=92.622k/s
DuplicateFinder/many_small_duplicates/threads_2/10000/4096/2/100/1/0_stddev                         0.251 ms         1.10 ms            3 bytes_per_second=3.76061M/s items_per_second=962.715/s
DuplicateFinder/many_small_duplicates/threads_2/10000/4096/2/100/1/0_cv                              0.11 %          1.03 %             3 bytes_per_second=1.03% items_per_second=1.03%
DuplicateFinder/many_small_duplicates/threads_4/10000/4096/4/100/1/0_mean                             166 ms         95.7 ms            3 bytes_per_second=417.45M/s items_per_second=106.867k/s
DuplicateFinder/many_small_duplicates/threads_4/10000/4096/4/100/1/0_median                           177 ms          105 ms            3 bytes_per_second=372.198M/s items_per_second=95.2826k/s
DuplicateFinder/many_small_duplicates/threads_4/10000/4096/4/100/1/0_stddev                          20.1 ms         16.6 ms            3 bytes_per_second=80.5271M/s items_per_second=20.6149k/s
DuplicateFinder/many_small_duplicates/threads_4/10000/4096/4/100/1/0_cv                             12.08 %         17.36 %             3 bytes_per_second=19.29% items_per_second=19.29%
DuplicateFinder/many_small_duplicates/threads_8/10000/4096/8/100/1/0_mean                             137 ms         91.8 ms            3 bytes_per_second=426.224M/s items_per_second=109.113k/s
DuplicateFinder/many_small_duplicates/threads_8/10000/4096/8/100/1/0_median                           140 ms         93.8 ms            3 bytes_per_second=416.632M/s items_per_second=106.658k/s
DuplicateFinder/many_small_duplicates/threads_8/10000/4096/8/100/1/0_stddev                          7.28 ms         4.24 ms            3 bytes_per_second=20.1968M/s items_per_second=5.17037k/s
DuplicateFinder/many_small_duplicates/threads_8/10000/4096/8/100/1/0_cv                              5.31 %          4.62 %             3 bytes_per_second=4.74% items_per_second=4.74%
DuplicateFinder/large_same_size_different_content/threads_1/20/8388608/1/0/1/0_mean                   480 ms        0.529 ms            3 bytes_per_second=307.372G/s items_per_second=39.3436k/s
DuplicateFinder/large_same_size_different_content/threads_1/20/8388608/1/0/1/0_median                 481 ms        0.595 ms            3 bytes_per_second=262.68G/s items_per_second=33.6231k/s
DuplicateFinder/large_same_size_different_content/threads_1/20/8388608/1/0/1/0_stddev                2.59 ms        0.119 ms            3 bytes_per_second=79.3521G/s items_per_second=10.1571k/s
DuplicateFinder/large_same_size_different_content/threads_1/20/8388608/1/0/1/0_cv                    0.54 %         22.47 %             3 bytes_per_second=25.82% items_per_second=25.82%
DuplicateFinder/large_same_size_different_content/threads_2/20/8388608/2/0/1/0_mean                   514 ms        0.577 ms            3 bytes_per_second=270.743G/s items_per_second=34.6551k/s
DuplicateFinder/large_same_size_different_content/threads_2/20/8388608/2/0/1/0_median                 515 ms        0.572 ms            3 bytes_per_second=273.008G/s items_per_second=34.945k/s
DuplicateFinder/large_same_size_different_content/threads_2/20/8388608/2/0/1/0_stddev                2.42 ms        0.009 ms            3 bytes_per_second=4.0137G/s items_per_second=513.754/s
DuplicateFinder/large_same_size_different_content/threads_2/20/8388608/2/0/1/0_cv                    0.47 %          1.50 %             3 bytes_per_second=1.48% items_per_second=1.48%
DuplicateFinder/large_same_size_different_content/threads_4/20/8388608/4/0/1/0_mean                   511 ms        0.584 ms            3 bytes_per_second=267.442G/s items_per_second=34.2326k/s
DuplicateFinder/large_same_size_different_content/threads_4/20/8388608/4/0/1/0_median                 513 ms        0.582 ms            3 bytes_per_second=268.594G/s items_per_second=34.3801k/s
DuplicateFinder/large_same_size_different_content/threads_4/20/8388608/4/0/1/0_stddev                5.03 ms        0.011 ms            3 bytes_per_second=4.77853G/s items_per_second=611.652/s
DuplicateFinder/large_same_size_different_content/threads_4/20/8388608/4/0/1/0_cv                    0.98 %          1.80 %             3 bytes_per_second=1.79% items_per_second=1.79%
DuplicateFinder/large_same_size_different_content/threads_8/20/8388608/8/0/1/0_mean                   510 ms        0.573 ms            3 bytes_per_second=272.731G/s items_per_second=34.9096k/s
DuplicateFinder/large_same_size_different_content/threads_8/20/8388608/8/0/1/0_median                 511 ms        0.575 ms            3 bytes_per_second=271.862G/s items_per_second=34.7983k/s
DuplicateFinder/large_same_size_different_content/threads_8/20/8388608/8/0/1/0_stddev               0.467 ms        0.004 ms            3 bytes_per_second=2.11179G/s items_per_second=270.309/s
DuplicateFinder/large_same_size_different_content/threads_8/20/8388608/8/0/1/0_cv                    0.09 %          0.77 %             3 bytes_per_second=0.77% items_per_second=0.77%
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_1/20/8388608/1/0/1/1_mean          483 ms        0.601 ms            3 bytes_per_second=259.857G/s items_per_second=33.2617k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_1/20/8388608/1/0/1/1_median        484 ms        0.604 ms            3 bytes_per_second=258.56G/s items_per_second=33.0957k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_1/20/8388608/1/0/1/1_stddev      0.207 ms        0.007 ms            3 bytes_per_second=3.00155G/s items_per_second=384.198/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_1/20/8388608/1/0/1/1_cv           0.04 %          1.15 %             3 bytes_per_second=1.16% items_per_second=1.16%
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_2/20/8388608/2/0/1/1_mean          512 ms        0.588 ms            3 bytes_per_second=266.102G/s items_per_second=34.061k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_2/20/8388608/2/0/1/1_median        512 ms        0.579 ms            3 bytes_per_second=269.808G/s items_per_second=34.5354k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_2/20/8388608/2/0/1/1_stddev      0.712 ms        0.020 ms            3 bytes_per_second=8.8013G/s items_per_second=1.12657k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_2/20/8388608/2/0/1/1_cv           0.14 %          3.37 %             3 bytes_per_second=3.31% items_per_second=3.31%
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_4/20/8388608/4/0/1/1_mean          515 ms        0.585 ms            3 bytes_per_second=267.333G/s items_per_second=34.2186k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_4/20/8388608/4/0/1/1_median        515 ms        0.587 ms            3 bytes_per_second=265.995G/s items_per_second=34.0473k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_4/20/8388608/4/0/1/1_stddev       1.75 ms        0.016 ms            3 bytes_per_second=7.15275G/s items_per_second=915.552/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_4/20/8388608/4/0/1/1_cv           0.34 %          2.66 %             3 bytes_per_second=2.68% items_per_second=2.68%
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_8/20/8388608/8/0/1/1_mean          512 ms        0.576 ms            3 bytes_per_second=271.218G/s items_per_second=34.7159k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_8/20/8388608/8/0/1/1_median        512 ms        0.576 ms            3 bytes_per_second=271.263G/s items_per_second=34.7216k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_8/20/8388608/8/0/1/1_stddev       1.21 ms        0.003 ms            3 bytes_per_second=1.50256G/s items_per_second=192.327/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_8/20/8388608/8/0/1/1_cv           0.24 %          0.55 %             3 bytes_per_second=0.55% items_per_second=0.55%
DuplicateFinder/large_duplicates/threads_1/20/8388608/1/100/1/0_mean                                  481 ms        0.656 ms            3 bytes_per_second=238.314G/s items_per_second=30.5042k/s
DuplicateFinder/large_duplicates/threads_1/20/8388608/1/100/1/0_median                                481 ms        0.655 ms            3 bytes_per_second=238.451G/s items_per_second=30.5218k/s
DuplicateFinder/large_duplicates/threads_1/20/8388608/1/100/1/0_stddev                               1.31 ms        0.006 ms            3 bytes_per_second=2.10792G/s items_per_second=269.814/s
DuplicateFinder/large_duplicates/threads_1/20/8388608/1/100/1/0_cv                                   0.27 %          0.89 %             3 bytes_per_second=0.88% items_per_second=0.88%
DuplicateFinder/large_duplicates/threads_2/20/8388608/2/100/1/0_mean                                  518 ms        0.621 ms            3 bytes_per_second=251.808G/s items_per_second=32.2314k/s
DuplicateFinder/large_duplicates/threads_2/20/8388608/2/100/1/0_median                                519 ms        0.618 ms            3 bytes_per_second=252.644G/s items_per_second=32.3384k/s
DuplicateFinder/large_duplicates/threads_2/20/8388608/2/100/1/0_stddev                               2.95 ms        0.011 ms            3 bytes_per_second=4.36132G/s items_per_second=558.249/s
DuplicateFinder/large_duplicates/threads_2/20/8388608/2/100/1/0_cv                                   0.57 %          1.74 %             3 bytes_per_second=1.73% items_per_second=1.73%
DuplicateFinder/large_duplicates/threads_4/20/8388608/4/100/1/0_mean                                  517 ms        0.631 ms            3 bytes_per_second=247.824G/s items_per_second=31.7215k/s
DuplicateFinder/large_duplicates/threads_4/20/8388608/4/100/1/0_median                                517 ms        0.628 ms            3 bytes_per_second=248.921G/s items_per_second=31.8619k/s
DuplicateFinder/large_duplicates/threads_4/20/8388608/4/100/1/0_stddev                              0.710 ms        0.011 ms            3 bytes_per_second=4.1606G/s items_per_second=532.557/s
DuplicateFinder/large_duplicates/threads_4/20/8388608/4/100/1/0_cv                                   0.14 %          1.69 %             3 bytes_per_second=1.68% items_per_second=1.68%
DuplicateFinder/large_duplicates/threads_8/20/8388608/8/100/1/0_mean                                  516 ms        0.620 ms            3 bytes_per_second=252.168G/s items_per_second=32.2775k/s
DuplicateFinder/large_duplicates/threads_8/20/8388608/8/100/1/0_median                                516 ms        0.621 ms            3 bytes_per_second=251.781G/s items_per_second=32.2279k/s
DuplicateFinder/large_duplicates/threads_8/20/8388608/8/100/1/0_stddev                              0.668 ms        0.002 ms            3 bytes_per_second=728.423M/s items_per_second=91.0528/s
DuplicateFinder/large_duplicates/threads_8/20/8388608/8/100/1/0_cv                                   0.13 %          0.28 %             3 bytes_per_second=0.28% items_per_second=0.28%
DuplicateFinder/mixed_realistic/threads_1/1000/-1/1/20/0/0_mean                                      3323 ms         16.5 ms            3 items_per_second=60.726k/s
DuplicateFinder/mixed_realistic/threads_1/1000/-1/1/20/0/0_median                                    3325 ms         16.5 ms            3 items_per_second=60.7183k/s
DuplicateFinder/mixed_realistic/threads_1/1000/-1/1/20/0/0_stddev                                    10.6 ms        0.259 ms            3 items_per_second=954.957/s
DuplicateFinder/mixed_realistic/threads_1/1000/-1/1/20/0/0_cv                                        0.32 %          1.57 %             3 items_per_second=1.57%
DuplicateFinder/mixed_realistic/threads_2/1000/-1/2/20/0/0_mean                                      1892 ms         16.3 ms            3 items_per_second=61.322k/s
DuplicateFinder/mixed_realistic/threads_2/1000/-1/2/20/0/0_median                                    1892 ms         16.3 ms            3 items_per_second=61.3419k/s
DuplicateFinder/mixed_realistic/threads_2/1000/-1/2/20/0/0_stddev                                   0.992 ms        0.035 ms            3 items_per_second=131.862/s
DuplicateFinder/mixed_realistic/threads_2/1000/-1/2/20/0/0_cv                                        0.05 %          0.22 %             3 items_per_second=0.22%
DuplicateFinder/mixed_realistic/threads_4/1000/-1/4/20/0/0_mean                                      1691 ms         15.0 ms            3 items_per_second=67.7779k/s
DuplicateFinder/mixed_realistic/threads_4/1000/-1/4/20/0/0_median                                    1692 ms         16.2 ms            3 items_per_second=61.7682k/s
DuplicateFinder/mixed_realistic/threads_4/1000/-1/4/20/0/0_stddev                                    1.87 ms         2.13 ms            3 items_per_second=10.5231k/s
DuplicateFinder/mixed_realistic/threads_4/1000/-1/4/20/0/0_cv                                        0.11 %         14.25 %             3 items_per_second=15.53%
DuplicateFinder/mixed_realistic/threads_8/1000/-1/8/20/0/0_mean                                      1505 ms         15.5 ms            3 items_per_second=64.7724k/s
DuplicateFinder/mixed_realistic/threads_8/1000/-1/8/20/0/0_median                                    1505 ms         16.1 ms            3 items_per_second=62.0349k/s
DuplicateFinder/mixed_realistic/threads_8/1000/-1/8/20/0/0_stddev                                    1.09 ms         1.15 ms            3 items_per_second=4.99791k/s
DuplicateFinder/mixed_realistic/threads_8/1000/-1/8/20/0/0_cv                                        0.07 %          7.39 %             3 items_per_second=7.72%
wxm@wxm-Precision-7920-Tower:~/FileSystemTools$ 
```

## V4 (大文件首尾哈希)

Commit Hash：4972c07ebd6e209486bebe4ba98a47c16ebf032c

```
wxm@wxm-Precision-7920-Tower:~/FileSystemTools$ ./build-bench/benchmark/dedup/dedupBenchmark \
  --benchmark_min_time=1.0 \
  --benchmark_repetitions=3 \
  --benchmark_report_aggregates_only=true
2026-06-25T10:37:27+08:00
Running ./build-bench/benchmark/dedup/dedupBenchmark
Run on (24 X 3500 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x12)
  L1 Instruction 32 KiB (x12)
  L2 Unified 1024 KiB (x12)
  L3 Unified 16896 KiB (x1)
Load Average: 1.23, 1.35, 1.86
***WARNING*** CPU scaling is enabled, the benchmark real time measurements may be noisy and will incur extra overhead.
---------------------------------------------------------------------------------------------------------------------------------------------------------
Benchmark                                                                                               Time             CPU   Iterations UserCounters...
---------------------------------------------------------------------------------------------------------------------------------------------------------
DuplicateFinder/100_files_4KiB/threads_1/100/4096/1/20/0/0_mean                                      2.51 ms         1.30 ms            3 bytes_per_second=309.434M/s items_per_second=79.2151k/s
DuplicateFinder/100_files_4KiB/threads_1/100/4096/1/20/0/0_median                                    2.77 ms         1.41 ms            3 bytes_per_second=276.612M/s items_per_second=70.8128k/s
DuplicateFinder/100_files_4KiB/threads_1/100/4096/1/20/0/0_stddev                                   0.496 ms        0.238 ms            3 bytes_per_second=63.5577M/s items_per_second=16.2708k/s
DuplicateFinder/100_files_4KiB/threads_1/100/4096/1/20/0/0_cv                                       19.76 %         18.41 %             3 bytes_per_second=20.54% items_per_second=20.54%
DuplicateFinder/100_files_4KiB/threads_2/100/4096/2/20/0/0_mean                                      2.84 ms         1.45 ms            3 bytes_per_second=268.86M/s items_per_second=68.8281k/s
DuplicateFinder/100_files_4KiB/threads_2/100/4096/2/20/0/0_median                                    2.84 ms         1.46 ms            3 bytes_per_second=268.47M/s items_per_second=68.7282k/s
DuplicateFinder/100_files_4KiB/threads_2/100/4096/2/20/0/0_stddev                                   0.035 ms        0.016 ms            3 bytes_per_second=3.03522M/s items_per_second=777.016/s
DuplicateFinder/100_files_4KiB/threads_2/100/4096/2/20/0/0_cv                                        1.24 %          1.13 %             3 bytes_per_second=1.13% items_per_second=1.13%
DuplicateFinder/100_files_4KiB/threads_4/100/4096/4/20/0/0_mean                                      2.83 ms         1.45 ms            3 bytes_per_second=269.867M/s items_per_second=69.0859k/s
DuplicateFinder/100_files_4KiB/threads_4/100/4096/4/20/0/0_median                                    2.87 ms         1.46 ms            3 bytes_per_second=267.225M/s items_per_second=68.4095k/s
DuplicateFinder/100_files_4KiB/threads_4/100/4096/4/20/0/0_stddev                                   0.073 ms        0.029 ms            3 bytes_per_second=5.52812M/s items_per_second=1.4152k/s
DuplicateFinder/100_files_4KiB/threads_4/100/4096/4/20/0/0_cv                                        2.58 %          2.03 %             3 bytes_per_second=2.05% items_per_second=2.05%
DuplicateFinder/100_files_4KiB/threads_8/100/4096/8/20/0/0_mean                                      2.79 ms         1.42 ms            3 bytes_per_second=274.563M/s items_per_second=70.2882k/s
DuplicateFinder/100_files_4KiB/threads_8/100/4096/8/20/0/0_median                                    2.83 ms         1.44 ms            3 bytes_per_second=271.164M/s items_per_second=69.418k/s
DuplicateFinder/100_files_4KiB/threads_8/100/4096/8/20/0/0_stddev                                   0.090 ms        0.035 ms            3 bytes_per_second=6.88126M/s items_per_second=1.7616k/s
DuplicateFinder/100_files_4KiB/threads_8/100/4096/8/20/0/0_cv                                        3.25 %          2.47 %             3 bytes_per_second=2.51% items_per_second=2.51%
DuplicateFinder/1000_files_4KiB/threads_1/1000/4096/1/20/0/0_mean                                    22.8 ms         12.1 ms            3 bytes_per_second=323.984M/s items_per_second=82.94k/s
DuplicateFinder/1000_files_4KiB/threads_1/1000/4096/1/20/0/0_median                                  22.8 ms         12.4 ms            3 bytes_per_second=315.345M/s items_per_second=80.7283k/s
DuplicateFinder/1000_files_4KiB/threads_1/1000/4096/1/20/0/0_stddev                                 0.519 ms        0.600 ms            3 bytes_per_second=16.5693M/s items_per_second=4.24175k/s
DuplicateFinder/1000_files_4KiB/threads_1/1000/4096/1/20/0/0_cv                                      2.28 %          4.97 %             3 bytes_per_second=5.11% items_per_second=5.11%
DuplicateFinder/1000_files_4KiB/threads_2/1000/4096/2/20/0/0_mean                                    19.3 ms         12.0 ms            3 bytes_per_second=325.013M/s items_per_second=83.2034k/s
DuplicateFinder/1000_files_4KiB/threads_2/1000/4096/2/20/0/0_median                                  19.3 ms         12.0 ms            3 bytes_per_second=325.562M/s items_per_second=83.3438k/s
DuplicateFinder/1000_files_4KiB/threads_2/1000/4096/2/20/0/0_stddev                                 0.044 ms        0.037 ms            3 bytes_per_second=1029.88k/s items_per_second=257.471/s
DuplicateFinder/1000_files_4KiB/threads_2/1000/4096/2/20/0/0_cv                                      0.23 %          0.31 %             3 bytes_per_second=0.31% items_per_second=0.31%
DuplicateFinder/1000_files_4KiB/threads_4/1000/4096/4/20/0/0_mean                                    11.1 ms         7.10 ms            3 bytes_per_second=549.959M/s items_per_second=140.789k/s
DuplicateFinder/1000_files_4KiB/threads_4/1000/4096/4/20/0/0_median                                  11.1 ms         7.13 ms            3 bytes_per_second=548.202M/s items_per_second=140.34k/s
DuplicateFinder/1000_files_4KiB/threads_4/1000/4096/4/20/0/0_stddev                                 0.108 ms        0.099 ms            3 bytes_per_second=7.66053M/s items_per_second=1.9611k/s
DuplicateFinder/1000_files_4KiB/threads_4/1000/4096/4/20/0/0_cv                                      0.97 %          1.39 %             3 bytes_per_second=1.39% items_per_second=1.39%
DuplicateFinder/1000_files_4KiB/threads_8/1000/4096/8/20/0/0_mean                                    11.2 ms         7.12 ms            3 bytes_per_second=548.423M/s items_per_second=140.396k/s
DuplicateFinder/1000_files_4KiB/threads_8/1000/4096/8/20/0/0_median                                  11.2 ms         7.16 ms            3 bytes_per_second=545.393M/s items_per_second=139.621k/s
DuplicateFinder/1000_files_4KiB/threads_8/1000/4096/8/20/0/0_stddev                                 0.136 ms        0.092 ms            3 bytes_per_second=7.14556M/s items_per_second=1.82926k/s
DuplicateFinder/1000_files_4KiB/threads_8/1000/4096/8/20/0/0_cv                                      1.22 %          1.29 %             3 bytes_per_second=1.30% items_per_second=1.30%
DuplicateFinder/10000_files_4KiB/threads_1/10000/4096/1/20/0/0_mean                                   137 ms         73.7 ms            3 bytes_per_second=530.582M/s items_per_second=135.829k/s
DuplicateFinder/10000_files_4KiB/threads_1/10000/4096/1/20/0/0_median                                 140 ms         74.4 ms            3 bytes_per_second=525.074M/s items_per_second=134.419k/s
DuplicateFinder/10000_files_4KiB/threads_1/10000/4096/1/20/0/0_stddev                                6.69 ms         2.01 ms            3 bytes_per_second=14.6933M/s items_per_second=3.76148k/s
DuplicateFinder/10000_files_4KiB/threads_1/10000/4096/1/20/0/0_cv                                    4.87 %          2.73 %             3 bytes_per_second=2.77% items_per_second=2.77%
DuplicateFinder/10000_files_4KiB/threads_2/10000/4096/2/20/0/0_mean                                  99.7 ms         61.1 ms            3 bytes_per_second=649.677M/s items_per_second=166.317k/s
DuplicateFinder/10000_files_4KiB/threads_2/10000/4096/2/20/0/0_median                                 108 ms         65.4 ms            3 bytes_per_second=597.618M/s items_per_second=152.99k/s
DuplicateFinder/10000_files_4KiB/threads_2/10000/4096/2/20/0/0_stddev                                17.0 ms         8.89 ms            3 bytes_per_second=102.949M/s items_per_second=26.355k/s
DuplicateFinder/10000_files_4KiB/threads_2/10000/4096/2/20/0/0_cv                                   17.04 %         14.55 %             3 bytes_per_second=15.85% items_per_second=15.85%
DuplicateFinder/10000_files_4KiB/threads_4/10000/4096/4/20/0/0_mean                                  84.7 ms         58.0 ms            3 bytes_per_second=674.161M/s items_per_second=172.585k/s
DuplicateFinder/10000_files_4KiB/threads_4/10000/4096/4/20/0/0_median                                85.3 ms         57.9 ms            3 bytes_per_second=674.733M/s items_per_second=172.732k/s
DuplicateFinder/10000_files_4KiB/threads_4/10000/4096/4/20/0/0_stddev                                1.92 ms         1.09 ms            3 bytes_per_second=12.6944M/s items_per_second=3.24976k/s
DuplicateFinder/10000_files_4KiB/threads_4/10000/4096/4/20/0/0_cv                                    2.27 %          1.89 %             3 bytes_per_second=1.88% items_per_second=1.88%
DuplicateFinder/10000_files_4KiB/threads_8/10000/4096/8/20/0/0_mean                                  69.8 ms         54.5 ms            3 bytes_per_second=716.34M/s items_per_second=183.383k/s
DuplicateFinder/10000_files_4KiB/threads_8/10000/4096/8/20/0/0_median                                69.0 ms         54.0 ms            3 bytes_per_second=722.916M/s items_per_second=185.066k/s
DuplicateFinder/10000_files_4KiB/threads_8/10000/4096/8/20/0/0_stddev                                1.70 ms         1.16 ms            3 bytes_per_second=15.0423M/s items_per_second=3.85083k/s
DuplicateFinder/10000_files_4KiB/threads_8/10000/4096/8/20/0/0_cv                                    2.44 %          2.12 %             3 bytes_per_second=2.10% items_per_second=2.10%
DuplicateFinder/1000_files_256KiB/threads_1/1000/262144/1/20/0/0_mean                                 176 ms         16.8 ms            3 bytes_per_second=14.563G/s items_per_second=59.6502k/s
DuplicateFinder/1000_files_256KiB/threads_1/1000/262144/1/20/0/0_median                               176 ms         16.8 ms            3 bytes_per_second=14.5541G/s items_per_second=59.6138k/s
DuplicateFinder/1000_files_256KiB/threads_1/1000/262144/1/20/0/0_stddev                             0.576 ms         1.37 ms            3 bytes_per_second=1.18129G/s items_per_second=4.83856k/s
DuplicateFinder/1000_files_256KiB/threads_1/1000/262144/1/20/0/0_cv                                  0.33 %          8.13 %             3 bytes_per_second=8.11% items_per_second=8.11%
DuplicateFinder/1000_files_256KiB/threads_2/1000/262144/2/20/0/0_mean                                97.1 ms         16.0 ms            3 bytes_per_second=15.3271G/s items_per_second=62.7798k/s
DuplicateFinder/1000_files_256KiB/threads_2/1000/262144/2/20/0/0_median                              96.9 ms         15.4 ms            3 bytes_per_second=15.8842G/s items_per_second=65.0618k/s
DuplicateFinder/1000_files_256KiB/threads_2/1000/262144/2/20/0/0_stddev                             0.836 ms         1.36 ms            3 bytes_per_second=1.24862G/s items_per_second=5.11435k/s
DuplicateFinder/1000_files_256KiB/threads_2/1000/262144/2/20/0/0_cv                                  0.86 %          8.52 %             3 bytes_per_second=8.15% items_per_second=8.15%
DuplicateFinder/1000_files_256KiB/threads_4/1000/262144/4/20/0/0_mean                                58.5 ms         15.6 ms            3 bytes_per_second=15.7103G/s items_per_second=64.3492k/s
DuplicateFinder/1000_files_256KiB/threads_4/1000/262144/4/20/0/0_median                              58.6 ms         15.8 ms            3 bytes_per_second=15.4169G/s items_per_second=63.1475k/s
DuplicateFinder/1000_files_256KiB/threads_4/1000/262144/4/20/0/0_stddev                              2.25 ms         1.49 ms            3 bytes_per_second=1.52761G/s items_per_second=6.25709k/s
DuplicateFinder/1000_files_256KiB/threads_4/1000/262144/4/20/0/0_cv                                  3.85 %          9.50 %             3 bytes_per_second=9.72% items_per_second=9.72%
DuplicateFinder/1000_files_256KiB/threads_8/1000/262144/8/20/0/0_mean                                37.1 ms         13.1 ms            3 bytes_per_second=18.821G/s items_per_second=77.0906k/s
DuplicateFinder/1000_files_256KiB/threads_8/1000/262144/8/20/0/0_median                              37.0 ms         12.9 ms            3 bytes_per_second=18.9519G/s items_per_second=77.6269k/s
DuplicateFinder/1000_files_256KiB/threads_8/1000/262144/8/20/0/0_stddev                              2.42 ms         1.87 ms            3 bytes_per_second=2.62632G/s items_per_second=10.7574k/s
DuplicateFinder/1000_files_256KiB/threads_8/1000/262144/8/20/0/0_cv                                  6.51 %         14.24 %             3 bytes_per_second=13.95% items_per_second=13.95%
DuplicateFinder/many_small_unique_same_size/threads_1/10000/4096/1/0/1/0_mean                         309 ms          106 ms            3 bytes_per_second=367.107M/s items_per_second=93.9795k/s
DuplicateFinder/many_small_unique_same_size/threads_1/10000/4096/1/0/1/0_median                       308 ms          106 ms            3 bytes_per_second=367.168M/s items_per_second=93.995k/s
DuplicateFinder/many_small_unique_same_size/threads_1/10000/4096/1/0/1/0_stddev                      6.82 ms         1.01 ms            3 bytes_per_second=3.49712M/s items_per_second=895.264/s
DuplicateFinder/many_small_unique_same_size/threads_1/10000/4096/1/0/1/0_cv                          2.21 %          0.95 %             3 bytes_per_second=0.95% items_per_second=0.95%
DuplicateFinder/many_small_unique_same_size/threads_2/10000/4096/2/0/1/0_mean                         219 ms          101 ms            3 bytes_per_second=388.005M/s items_per_second=99.3292k/s
DuplicateFinder/many_small_unique_same_size/threads_2/10000/4096/2/0/1/0_median                       221 ms          102 ms            3 bytes_per_second=382.189M/s items_per_second=97.8403k/s
DuplicateFinder/many_small_unique_same_size/threads_2/10000/4096/2/0/1/0_stddev                      5.32 ms         3.05 ms            3 bytes_per_second=11.9677M/s items_per_second=3.06374k/s
DuplicateFinder/many_small_unique_same_size/threads_2/10000/4096/2/0/1/0_cv                          2.43 %          3.03 %             3 bytes_per_second=3.08% items_per_second=3.08%
DuplicateFinder/many_small_unique_same_size/threads_4/10000/4096/4/0/1/0_mean                         145 ms         80.3 ms            3 bytes_per_second=491.834M/s items_per_second=125.91k/s
DuplicateFinder/many_small_unique_same_size/threads_4/10000/4096/4/0/1/0_median                       138 ms         74.6 ms            3 bytes_per_second=523.621M/s items_per_second=134.047k/s
DuplicateFinder/many_small_unique_same_size/threads_4/10000/4096/4/0/1/0_stddev                      15.1 ms         10.8 ms            3 bytes_per_second=61.2578M/s items_per_second=15.682k/s
DuplicateFinder/many_small_unique_same_size/threads_4/10000/4096/4/0/1/0_cv                         10.40 %         13.41 %             3 bytes_per_second=12.45% items_per_second=12.45%
DuplicateFinder/many_small_unique_same_size/threads_8/10000/4096/8/0/1/0_mean                         130 ms         85.1 ms            3 bytes_per_second=460.803M/s items_per_second=117.966k/s
DuplicateFinder/many_small_unique_same_size/threads_8/10000/4096/8/0/1/0_median                       132 ms         87.7 ms            3 bytes_per_second=445.461M/s items_per_second=114.038k/s
DuplicateFinder/many_small_unique_same_size/threads_8/10000/4096/8/0/1/0_stddev                      8.52 ms         6.61 ms            3 bytes_per_second=37.2122M/s items_per_second=9.52632k/s
DuplicateFinder/many_small_unique_same_size/threads_8/10000/4096/8/0/1/0_cv                          6.56 %          7.76 %             3 bytes_per_second=8.08% items_per_second=8.08%
DuplicateFinder/many_small_duplicates/threads_1/10000/4096/1/100/1/0_mean                             316 ms          111 ms            3 bytes_per_second=352.809M/s items_per_second=90.319k/s
DuplicateFinder/many_small_duplicates/threads_1/10000/4096/1/100/1/0_median                           316 ms          110 ms            3 bytes_per_second=353.923M/s items_per_second=90.6042k/s
DuplicateFinder/many_small_duplicates/threads_1/10000/4096/1/100/1/0_stddev                          4.93 ms        0.640 ms            3 bytes_per_second=2.03361M/s items_per_second=520.604/s
DuplicateFinder/many_small_duplicates/threads_1/10000/4096/1/100/1/0_cv                              1.56 %          0.58 %             3 bytes_per_second=0.58% items_per_second=0.58%
DuplicateFinder/many_small_duplicates/threads_2/10000/4096/2/100/1/0_mean                             222 ms          105 ms            3 bytes_per_second=370.447M/s items_per_second=94.8344k/s
DuplicateFinder/many_small_duplicates/threads_2/10000/4096/2/100/1/0_median                           222 ms          105 ms            3 bytes_per_second=370.856M/s items_per_second=94.9391k/s
DuplicateFinder/many_small_duplicates/threads_2/10000/4096/2/100/1/0_stddev                          2.42 ms         1.35 ms            3 bytes_per_second=4.72092M/s items_per_second=1.20855k/s
DuplicateFinder/many_small_duplicates/threads_2/10000/4096/2/100/1/0_cv                              1.09 %          1.28 %             3 bytes_per_second=1.27% items_per_second=1.27%
DuplicateFinder/many_small_duplicates/threads_4/10000/4096/4/100/1/0_mean                             175 ms          103 ms            3 bytes_per_second=378.462M/s items_per_second=96.8862k/s
DuplicateFinder/many_small_duplicates/threads_4/10000/4096/4/100/1/0_median                           177 ms          104 ms            3 bytes_per_second=376.021M/s items_per_second=96.2613k/s
DuplicateFinder/many_small_duplicates/threads_4/10000/4096/4/100/1/0_stddev                          4.15 ms         1.73 ms            3 bytes_per_second=6.39671M/s items_per_second=1.63756k/s
DuplicateFinder/many_small_duplicates/threads_4/10000/4096/4/100/1/0_cv                              2.37 %          1.68 %             3 bytes_per_second=1.69% items_per_second=1.69%
DuplicateFinder/many_small_duplicates/threads_8/10000/4096/8/100/1/0_mean                             132 ms         88.0 ms            3 bytes_per_second=445.025M/s items_per_second=113.926k/s
DuplicateFinder/many_small_duplicates/threads_8/10000/4096/8/100/1/0_median                           130 ms         86.2 ms            3 bytes_per_second=452.931M/s items_per_second=115.95k/s
DuplicateFinder/many_small_duplicates/threads_8/10000/4096/8/100/1/0_stddev                          5.71 ms         4.90 ms            3 bytes_per_second=24.1761M/s items_per_second=6.18907k/s
DuplicateFinder/many_small_duplicates/threads_8/10000/4096/8/100/1/0_cv                              4.32 %          5.57 %             3 bytes_per_second=5.43% items_per_second=5.43%
DuplicateFinder/large_same_size_different_content/threads_1/20/8388608/1/0/1/0_mean                  1.02 ms        0.372 ms            3 bytes_per_second=420.114G/s items_per_second=53.7747k/s
DuplicateFinder/large_same_size_different_content/threads_1/20/8388608/1/0/1/0_median                1.01 ms        0.372 ms            3 bytes_per_second=419.999G/s items_per_second=53.7599k/s
DuplicateFinder/large_same_size_different_content/threads_1/20/8388608/1/0/1/0_stddev               0.008 ms        0.001 ms            3 bytes_per_second=1.52701G/s items_per_second=195.457/s
DuplicateFinder/large_same_size_different_content/threads_1/20/8388608/1/0/1/0_cv                    0.77 %          0.36 %             3 bytes_per_second=0.36% items_per_second=0.36%
DuplicateFinder/large_same_size_different_content/threads_2/20/8388608/2/0/1/0_mean                  1.71 ms        0.470 ms            3 bytes_per_second=338.063G/s items_per_second=43.272k/s
DuplicateFinder/large_same_size_different_content/threads_2/20/8388608/2/0/1/0_median                1.95 ms        0.509 ms            3 bytes_per_second=307.186G/s items_per_second=39.3198k/s
DuplicateFinder/large_same_size_different_content/threads_2/20/8388608/2/0/1/0_stddev               0.478 ms        0.070 ms            3 bytes_per_second=54.977G/s items_per_second=7.03706k/s
DuplicateFinder/large_same_size_different_content/threads_2/20/8388608/2/0/1/0_cv                   27.91 %         14.87 %             3 bytes_per_second=16.26% items_per_second=16.26%
DuplicateFinder/large_same_size_different_content/threads_4/20/8388608/4/0/1/0_mean                  1.86 ms        0.478 ms            3 bytes_per_second=329.363G/s items_per_second=42.1585k/s
DuplicateFinder/large_same_size_different_content/threads_4/20/8388608/4/0/1/0_median                1.95 ms        0.506 ms            3 bytes_per_second=308.52G/s items_per_second=39.4906k/s
DuplicateFinder/large_same_size_different_content/threads_4/20/8388608/4/0/1/0_stddev               0.187 ms        0.051 ms            3 bytes_per_second=37.4988G/s items_per_second=4.79984k/s
DuplicateFinder/large_same_size_different_content/threads_4/20/8388608/4/0/1/0_cv                   10.03 %         10.68 %             3 bytes_per_second=11.39% items_per_second=11.39%
DuplicateFinder/large_same_size_different_content/threads_8/20/8388608/8/0/1/0_mean                  1.93 ms        0.480 ms            3 bytes_per_second=325.59G/s items_per_second=41.6755k/s
DuplicateFinder/large_same_size_different_content/threads_8/20/8388608/8/0/1/0_median                1.97 ms        0.487 ms            3 bytes_per_second=320.887G/s items_per_second=41.0735k/s
DuplicateFinder/large_same_size_different_content/threads_8/20/8388608/8/0/1/0_stddev               0.066 ms        0.012 ms            3 bytes_per_second=8.38126G/s items_per_second=1072.8/s
DuplicateFinder/large_same_size_different_content/threads_8/20/8388608/8/0/1/0_cv                    3.43 %          2.54 %             3 bytes_per_second=2.57% items_per_second=2.57%
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_1/20/8388608/1/0/1/1_mean        0.995 ms        0.361 ms            3 bytes_per_second=432.305G/s items_per_second=55.3351k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_1/20/8388608/1/0/1/1_median      0.988 ms        0.360 ms            3 bytes_per_second=434.152G/s items_per_second=55.5714k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_1/20/8388608/1/0/1/1_stddev      0.016 ms        0.003 ms            3 bytes_per_second=3.50536G/s items_per_second=448.686/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_1/20/8388608/1/0/1/1_cv           1.57 %          0.81 %             3 bytes_per_second=0.81% items_per_second=0.81%
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_2/20/8388608/2/0/1/1_mean         1.59 ms        0.447 ms            3 bytes_per_second=350.084G/s items_per_second=44.8108k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_2/20/8388608/2/0/1/1_median       1.59 ms        0.440 ms            3 bytes_per_second=355.446G/s items_per_second=45.4971k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_2/20/8388608/2/0/1/1_stddev      0.139 ms        0.026 ms            3 bytes_per_second=19.6216G/s items_per_second=2.51156k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_2/20/8388608/2/0/1/1_cv           8.73 %          5.73 %             3 bytes_per_second=5.60% items_per_second=5.60%
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_4/20/8388608/4/0/1/1_mean         1.82 ms        0.486 ms            3 bytes_per_second=322.704G/s items_per_second=41.3062k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_4/20/8388608/4/0/1/1_median       1.94 ms        0.504 ms            3 bytes_per_second=310.076G/s items_per_second=39.6897k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_4/20/8388608/4/0/1/1_stddev      0.248 ms        0.032 ms            3 bytes_per_second=22.4202G/s items_per_second=2.86978k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_4/20/8388608/4/0/1/1_cv          13.58 %          6.68 %             3 bytes_per_second=6.95% items_per_second=6.95%
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_8/20/8388608/8/0/1/1_mean         1.97 ms        0.501 ms            3 bytes_per_second=312.062G/s items_per_second=39.9439k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_8/20/8388608/8/0/1/1_median       1.99 ms        0.499 ms            3 bytes_per_second=313.082G/s items_per_second=40.0745k/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_8/20/8388608/8/0/1/1_stddev      0.027 ms        0.003 ms            3 bytes_per_second=1.94332G/s items_per_second=248.745/s
DuplicateFinder/large_same_size_same_prefix_different_tail/threads_8/20/8388608/8/0/1/1_cv           1.34 %          0.62 %             3 bytes_per_second=0.62% items_per_second=0.62%
DuplicateFinder/large_duplicates/threads_1/20/8388608/1/100/1/0_mean                                  485 ms         1.91 ms            3 bytes_per_second=81.9539G/s items_per_second=10.4901k/s
DuplicateFinder/large_duplicates/threads_1/20/8388608/1/100/1/0_median                                485 ms         1.90 ms            3 bytes_per_second=82.1932G/s items_per_second=10.5207k/s
DuplicateFinder/large_duplicates/threads_1/20/8388608/1/100/1/0_stddev                               1.05 ms        0.010 ms            3 bytes_per_second=436.4M/s items_per_second=54.55/s
DuplicateFinder/large_duplicates/threads_1/20/8388608/1/100/1/0_cv                                   0.22 %          0.52 %             3 bytes_per_second=0.52% items_per_second=0.52%
DuplicateFinder/large_duplicates/threads_2/20/8388608/2/100/1/0_mean                                  245 ms         1.43 ms            3 bytes_per_second=109.598G/s items_per_second=14.0286k/s
DuplicateFinder/large_duplicates/threads_2/20/8388608/2/100/1/0_median                                245 ms         1.43 ms            3 bytes_per_second=109.202G/s items_per_second=13.9778k/s
DuplicateFinder/large_duplicates/threads_2/20/8388608/2/100/1/0_stddev                               1.21 ms        0.061 ms            3 bytes_per_second=4.69078G/s items_per_second=600.42/s
DuplicateFinder/large_duplicates/threads_2/20/8388608/2/100/1/0_cv                                   0.49 %          4.26 %             3 bytes_per_second=4.28% items_per_second=4.28%
DuplicateFinder/large_duplicates/threads_4/20/8388608/4/100/1/0_mean                                  126 ms         1.13 ms            3 bytes_per_second=139.643G/s items_per_second=17.8742k/s
DuplicateFinder/large_duplicates/threads_4/20/8388608/4/100/1/0_median                                126 ms         1.18 ms            3 bytes_per_second=132.737G/s items_per_second=16.9903k/s
DuplicateFinder/large_duplicates/threads_4/20/8388608/4/100/1/0_stddev                              0.971 ms        0.112 ms            3 bytes_per_second=14.7244G/s items_per_second=1.88472k/s
DuplicateFinder/large_duplicates/threads_4/20/8388608/4/100/1/0_cv                                   0.77 %          9.97 %             3 bytes_per_second=10.54% items_per_second=10.54%
DuplicateFinder/large_duplicates/threads_8/20/8388608/8/100/1/0_mean                                 78.5 ms         1.06 ms            3 bytes_per_second=148.297G/s items_per_second=18.982k/s
DuplicateFinder/large_duplicates/threads_8/20/8388608/8/100/1/0_median                               78.5 ms         1.05 ms            3 bytes_per_second=149.126G/s items_per_second=19.0882k/s
DuplicateFinder/large_duplicates/threads_8/20/8388608/8/100/1/0_stddev                              0.216 ms        0.129 ms            3 bytes_per_second=17.7226G/s items_per_second=2.26849k/s
DuplicateFinder/large_duplicates/threads_8/20/8388608/8/100/1/0_cv                                   0.27 %         12.14 %             3 bytes_per_second=11.95% items_per_second=11.95%
DuplicateFinder/mixed_realistic/threads_1/1000/-1/1/20/0/0_mean                                      2538 ms         22.8 ms            3 items_per_second=44.3006k/s
DuplicateFinder/mixed_realistic/threads_1/1000/-1/1/20/0/0_median                                    2541 ms         24.4 ms            3 items_per_second=40.9718k/s
DuplicateFinder/mixed_realistic/threads_1/1000/-1/1/20/0/0_stddev                                    8.37 ms         2.77 ms            3 items_per_second=5.77451k/s
DuplicateFinder/mixed_realistic/threads_1/1000/-1/1/20/0/0_cv                                        0.33 %         12.12 %             3 items_per_second=13.03%
DuplicateFinder/mixed_realistic/threads_2/1000/-1/2/20/0/0_mean                                      1287 ms         22.2 ms            3 items_per_second=45.0987k/s
DuplicateFinder/mixed_realistic/threads_2/1000/-1/2/20/0/0_median                                    1287 ms         22.7 ms            3 items_per_second=43.9565k/s
DuplicateFinder/mixed_realistic/threads_2/1000/-1/2/20/0/0_stddev                                    4.45 ms         1.06 ms            3 items_per_second=2.2119k/s
DuplicateFinder/mixed_realistic/threads_2/1000/-1/2/20/0/0_cv                                        0.35 %          4.77 %             3 items_per_second=4.90%
DuplicateFinder/mixed_realistic/threads_4/1000/-1/4/20/0/0_mean                                       671 ms         19.1 ms            3 items_per_second=53.1542k/s
DuplicateFinder/mixed_realistic/threads_4/1000/-1/4/20/0/0_median                                     670 ms         19.9 ms            3 items_per_second=50.1311k/s
DuplicateFinder/mixed_realistic/threads_4/1000/-1/4/20/0/0_stddev                                    4.95 ms         2.78 ms            3 items_per_second=8.27232k/s
DuplicateFinder/mixed_realistic/threads_4/1000/-1/4/20/0/0_cv                                        0.74 %         14.55 %             3 items_per_second=15.56%
DuplicateFinder/mixed_realistic/threads_8/1000/-1/8/20/0/0_mean                                       358 ms         13.3 ms            3 items_per_second=75.6207k/s
DuplicateFinder/mixed_realistic/threads_8/1000/-1/8/20/0/0_median                                     359 ms         13.4 ms            3 items_per_second=74.5727k/s
DuplicateFinder/mixed_realistic/threads_8/1000/-1/8/20/0/0_stddev                                    1.42 ms        0.769 ms            3 items_per_second=4.47033k/s
DuplicateFinder/mixed_realistic/threads_8/1000/-1/8/20/0/0_cv                                        0.40 %          5.80 %             3 items_per_second=5.91%
wxm@wxm-Precision-7920-Tower:~/FileSystemTools$ 
```

## V5（硬链接折叠与 I/O 局部性优化）

Commit Hash：17d2aeac4c3495d26b64607c66bacaaa89a067e1

无 BenchMark。
