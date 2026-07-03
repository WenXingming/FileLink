# FileLink

FileLink 正在按小步、可验证的方式重写为团队大文件分发与存储平台。

## 当前状态

- `src/dedup` 是已经稳定的离线重复文件发现引擎。
- `tools/dedup` 提供可独立运行的 dedup 命令行工具。
- 旧在线服务保存在 `old/server`，仅供理解历史行为，不参与构建。
- 新在线服务尚未开始实现；仓库不会用空壳 target 冒充可用功能。

## 当前目录

```text
FileLink/
├── src/dedup/          # 稳定的去重核心
├── tools/dedup/        # dedup CLI
├── tests/dedup/        # dedup 测试
├── benchmarks/dedup/   # dedup 性能基准
├── docs/dedup/         # dedup 设计与评测记录
└── old/
    └── server/         # 旧在线服务，仅供参考
```

## 构建与测试

```bash
cmake -S . -B build -DFILELINK_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

运行 dedup：

```bash
./build/tools/dedup/filelink-dedup /path/to/directory
```

## 重写原则

每次只实现一个可测试的业务切片。没有真实的第二种实现之前不引入接口、工厂或扩展层；旧代码只能帮助理解历史行为，不能直接复制到新实现。
