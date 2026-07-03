# FileLink

FileLink 正在按小步、可验证的方式重写为团队大文件分发与存储平台。

## 当前状态

- `src/storage` 提供本地内容寻址对象的原子提交与重复复用。
- `src/server` 提供基于 Tudou 的在线服务入口，目前已支持健康检查。
- 离线重复文件去重能力 (dedup) 已作为依赖通过 FetchContent 从 `FileSystemTools` 引入。
- 旧在线服务保存在 `old/server`，仅供理解历史行为，不参与构建。

## 当前目录

```text
FileLink/
├── src/                # 所有业务逻辑和入口（配置、存储、服务器应用等）
├── tests/              # 自动化测试
├── docs/               # 架构设计与评测记录
└── old/
    └── server/         # 旧在线服务，仅供参考
```

## 构建与测试

```bash
cmake -S . -B build -DFILELINK_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

运行配置好后的服务：

```bash
./build/apps/server/filelink-server
```

## 重写原则

每次只实现一个可测试的业务切片。没有真实的第二种实现之前不引入接口、工厂或扩展层；旧代码只能帮助理解历史行为，不能直接复制到新实现。
