# FileLink MySQL 客户端技术选型

## 1. 选型背景

FileLink 使用 MySQL/InnoDB 保存上传会话、逻辑文件、分享链接、权限和审计数据。客户端库需要满足：

- 连接 MySQL 8 的经典协议端口 `3306`。
- 支持 prepared statement、事务和 affected rows。
- 能够明确管理连接生命周期和多线程边界。
- 与当前 C++14 工程兼容。
- 构建方式稳定，不能为了一个客户端库拉入完整 MySQL Server。
- 业务代码不直接散落 `MYSQL*`、资源释放和错误处理。

这里选择的是“如何连接 MySQL”，不是数据库本身的选型。

## 2. 候选方案

| 方案 | 优点 | 主要问题 | 结论 |
|---|---|---|---|
| 官方 `libmysqlclient` C API | 官方经典协议实现；API 稳定；支持 prepared statement、事务、TLS 和多线程；系统包成熟 | API 偏底层，需要少量 RAII 包装 | 采用 |
| MySQL Connector/C++ X DevAPI | 现代 C++ API；官方维护；源码支持作为 CMake 子项目 | 当前版本要求 C++17；默认走 X Protocol；构建依赖和体积更大 | 不采用 |
| MySQL Connector/C++ legacy JDBC | C++ 接口直观；走经典协议 | 默认不构建；开启 `WITH_JDBC` 后仍依赖 `libmysqlclient`，只是又增加一层包装 | 不采用 |
| MariaDB Connector/C 或 C++ | 独立仓库、较容易源码构建；兼容 MySQL 协议 | 驱动供应商和目标数据库不一致；认证、TLS、错误码和行为可能存在差异 | 不作为静默兜底 |
| ORM | 可以减少部分 SQL 样板代码 | 状态 CAS、affected rows 和故障恢复仍要理解 SQL；额外抽象会遮挡项目重点 | 当前不采用 |

## 3. 最终决定

使用官方 `libmysqlclient` C API，并在 FileLink 内部建立一层很薄的 C++ RAII 边界。

```text
UploadSessionStore
        ↓
MySqlConnection / PreparedStatement
        ↓
libmysqlclient
        ↓
MySQL 8 / InnoDB
```

这层包装只负责：

- 初始化和关闭客户端资源。
- 建立、检测和关闭连接。
- 管理 prepared statement 生命周期。
- 将 MySQL 错误转换为不泄露密码的 C++ 异常。
- 保证一个连接同一时刻只由一个工作线程使用。

不建立通用 ORM、SQL Builder、Repository 基类或数据库无关接口。FileLink 只有一个真实数据库实现，提前抽象第二套数据库没有收益。

## 4. 为什么不用 Connector/C++

### 4.1 X DevAPI 与当前需求不匹配

官方 Connector/C++ 默认提供 X DevAPI，当前源码开启 C++17，并主要面向 MySQL X Protocol。FileLink 当前部署和面试讨论都围绕经典 MySQL SQL 协议、InnoDB 事务和端口 `3306`，没有使用文档存储 API 的需求。

为了一个数据库客户端把整个项目从 C++14 升级到 C++17 并不是不能做，而是当前没有业务收益。

### 4.2 legacy JDBC 没有消除底层依赖

Connector/C++ 的传统 JDBC 风格接口默认关闭，需要 `WITH_JDBC=ON`。该模式仍然通过 MySQL client library 访问经典协议，因此系统最终仍要提供 `libmysqlclient`。这会形成：

```text
FileLink
→ Connector/C++ JDBC wrapper
→ libmysqlclient
→ MySQL
```

相比直接在项目边界写一个只包含所需能力的 RAII 封装，它增加了构建和版本管理成本，却没有减少底层依赖。

## 5. FetchContent 可行性

### 5.1 能否 FetchContent Connector/C++

技术上可以。官方源码会识别自己是顶层项目还是子项目，因此可以被 `FetchContent_MakeAvailable()` 添加。

但不推荐用于 FileLink：

- 默认构建 X DevAPI，而不是本项目需要的经典协议封装。
- 会启用 C++17。
- 涉及 OpenSSL、Boost、Protobuf、压缩库等构建依赖。
- 启用 legacy JDBC 后仍需要 `libmysqlclient`。
- 会显著增加首次配置、编译时间和 CMake target 数量。

### 5.2 能否 FetchContent libmysqlclient

没有适合本项目的官方轻量独立仓库。`libmysqlclient` 随 MySQL 源码和开发包发布；FetchContent 官方 `mysql-server` 仓库等于为了一个客户端库引入整个数据库服务端源码，不是合理的依赖管理。

### 5.3 能否用 MariaDB Connector 作为 fallback

技术上可以，但不应该静默执行。fallback 不应改变数据库驱动实现，否则开发机和 CI 可能分别使用 Oracle 与 MariaDB 客户端，产生认证插件、TLS、错误码或边界行为差异。

如果未来明确决定使用 MariaDB Connector，应该把它作为一次独立技术选型，而不是 `libmysqlclient` 找不到时的隐式替代品。

## 6. 推荐的 CMake 集成

使用官方开发包提供的 `mysqlclient.pc`：

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(MySQLClient REQUIRED IMPORTED_TARGET mysqlclient)

target_link_libraries(filelink_lib PRIVATE
    PkgConfig::MySQLClient
)
```

开发环境和 CI 明确安装 MySQL client 开发包，例如 Ubuntu 上的 `libmysqlclient-dev`。运行环境只需要对应的动态库。

不提供 FetchContent fallback。依赖缺失时让 CMake 尽早失败，并给出明确安装提示，比下载并编译完整 MySQL Server 更可控。

为了获得可重复构建，后续应把开发依赖写入构建镜像或 CI 配置，而不是要求每个开发者手工猜测包名和版本。

## 7. 多线程与连接池边界

MySQL 客户端库需要在工作线程启动前完成全局初始化。每个调用 MySQL API 的线程需要遵循客户端线程初始化规则。

连接池遵循：

- 一个连接同一时刻只借给一个线程。
- 池大小有上限，获取连接有超时。
- 文件写入、BLAKE3 和 `fdatasync` 不占用连接。
- 只在执行短 SQL 时借出连接，用完立即归还。
- 自动重连不能掩盖“事务是否已经提交未知”的状态，连接中断后必须重新查询业务状态。

第一步只实现单连接 RAII；验证连接生命周期后，再实现连接池。

### 7.1 当前集成测试

普通测试不应强制依赖正在运行的 MySQL。设置测试密码后才启用真实连接测试：

```bash
FILELINK_TEST_MYSQL_PASSWORD=<password> \
ctest --test-dir build -R MySqlConnectionTest --output-on-failure
```

端口不是默认的 `3306` 时可以额外设置 `FILELINK_TEST_MYSQL_PORT`。未设置密码变量时，参数校验测试照常运行，真实连接和错误认证用例显示为 skipped。

## 8. 面试回答

### 为什么不用更现代的 Connector/C++？

现代不等于适合。Connector/C++ 默认的 X DevAPI、C++17 和较重构建链路不是当前需求；传统 JDBC 模式仍依赖 `libmysqlclient`。因此直接采用官方经典协议客户端，在项目边界封装最小 RAII 层，依赖更少，事务和错误语义也更透明。

### 为什么不用 FetchContent？

FetchContent 适合能够作为独立 CMake 子项目构建的源码库。`libmysqlclient` 没有轻量官方独立仓库，拉取 `mysql-server` 会引入完整服务端源码。这里使用系统开发包，并通过构建镜像固定版本，比“为了形式统一而源码编译整个 MySQL”更可维护。

### 使用 C API 是否显得技术落后？

不会。技术价值在连接池、prepared statement、事务边界、条件更新、提交结果未知和故障恢复，而不是 API 的语法外观。C API 是官方经典协议实现，薄 RAII 层可以获得 C++ 的资源安全，同时保留底层语义透明度。

## 9. 参考资料

- MySQL C API：<https://dev.mysql.com/doc/c-api/8.4/en/>
- 使用 pkg-config 构建 MySQL 客户端：<https://dev.mysql.com/doc/c-api/8.4/en/c-api-building-clients-pkg-config.html>
- MySQL C API prepared statement：<https://dev.mysql.com/doc/c-api/8.4/en/c-api-prepared-statement-interface-usage.html>
- MySQL C API 多线程规则：<https://dev.mysql.com/doc/c-api/8.4/en/c-api-threaded-clients.html>
- MySQL Connector/C++ 源码构建：<https://dev.mysql.com/doc/connector-cpp/9.7/en/connector-cpp-installation-source-cpp.html>
- MySQL Connector/C++ 官方源码：<https://github.com/mysql/mysql-connector-cpp>
