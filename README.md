# FileLink

FileLink 正在按小步、可验证的方式重写为团队大文件分发与存储平台。

## 当前状态

- `src/dedup` 包含稳定的离线重复文件发现引擎及其命令行入口。
- `src/server` 提供基于 Tudou 的在线服务入口，目前已支持健康检查。
- 旧在线服务保存在 `old/server`，仅供理解历史行为，不参与构建。

## 当前目录

```text
FileLink/
├── src/
│   ├── config/         # 应用配置
│   ├── dedup/          # 去重核心与 CLI
│   └── server/         # 在线服务入口
├── tests/              # 自动化测试
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
./build/src/dedup/filelink-dedup /path/to/directory
```

## 重写原则

每次只实现一个可测试的业务切片。没有真实的第二种实现之前不引入接口、工厂或扩展层；旧代码只能帮助理解历史行为，不能直接复制到新实现。
