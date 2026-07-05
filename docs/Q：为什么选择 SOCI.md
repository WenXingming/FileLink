# 为什么 FileLink 选择 SOCI

FileLink 选择 `SOCI + libmysqlclient`，因为数据库访问不是项目需要重复实现的核心能力。SOCI 已经提供：

1. **C++ 风格的 SQL 语句的参数绑定、连接生命周期的 RAII 管理**
2. **连接池**
3. **事务管理**

等等，让我们把代码集中在上传状态机、存储一致性和 SQL 设计上。

```text
UploadService
    -> UploadSessionStore
    -> SOCI
    -> soci_mysql
    -> libmysqlclient
    -> MySQL
```

`libmysqlclient` 仍然是实际连接 MySQL 的官方客户端；SOCI 是其上的轻量 C++ 访问层，不是 ORM，也不会隐藏 SQL、索引或事务边界。

## 为什么不使用 libmysqlclient

直接使用 C API 意味着项目还要自行维护：

- **SQL 语句复杂度的参数绑定和结果类型转换，自行实现`MYSQL*` 等资源的 RAII 封装**；
- **自行实现连接池，还需处理并发、归还和关闭**；
- 事务异常路径上的回滚；
- 客户端库初始化及错误处理。

这些代码并不能形成 FileLink 的业务优势，却会增加并发和资源生命周期风险。使用成熟库是主动缩小维护面，而不是回避底层原理。

## 为什么当前不选择 Connector/C++

我们不是因为 Connector/C++ 不成熟而排除它，而是分别评估了它的两套 API：

- **X DevAPI** 自带现代 Session Pool，也支持关系表和 Document Store；代价是**引入 X Protocol、33060 端口**和另一套会话模型。
- **Classic JDBC 风格 API** 可以继续使用 3306 和普通 SQL，但应用**仍要组织连接池**等。

FileLink 真正需要的是：保留显式 SQL 参数绑定、一个有上限的连接池，RAII 事务、使用 Classic Protocol 等。SOCI 正好补齐这些能力，底层仍使用 MySQL 官方 `libmysqlclient`。因此我们没有用社区实现替换官方协议驱动，只是在官方驱动之上选择了更适合当前 SQL 模型的 C++ 访问层。

这项选择也有明确边界：如果以后需要 X Protocol、Document Store，或者 SOCI 无法暴露某项关键的 MySQL 专有能力，我们会在`UploadSessionStore` 边界重新评估 Connector/C++。在这些需求出现前，切换只会增加迁移和部署成本，不会增加业务能力。

### 面试追问：官方 Connector/C++ 是否更可靠？

官方维护是 Connector/C++ 的优势，但“官方”不是唯一决策条件。FileLink 的认证、TLS、网络协议和服务端兼容性仍由官方 `libmysqlclient` 提供；SOCI 负责的只是连接生命周期、绑定和事务封装。我们通过固定版本和 MySQL 集成测试控制这一层风险，同时避免维护自研连接池和 C API 资源管理代码。

## 面试回答

> 我们不是因为 Connector/C++ 不好而选择 SOCI，而是按需求做了分层选择。
>
> 1. X DevAPI 的优势是 X Protocol、Document Store 和原生 Session Pool，但项目只用 3306 上的关系型 SQL；
> 2. Classic API 虽然能满足协议要求，却仍需要组织连接池并编写 JDBC 风格的结果读取代码。
> 3. SOCI 在官方 libmysqlclient 之上直接提供参数绑定、RAII 事务和连接池，既保留显式 SQL，又删除了非核心的资源管理代码。以后真正需要 X Protocol 或 MySQL 专有能力时，可以在 Store 边界替换；现在切换不会增加业务能力。

## 参考

- [SOCI MySQL backend](https://soci.sourceforge.net/doc/master/backends/mysql/)
- [SOCI transactions](https://soci.sourceforge.net/doc/master/transactions/)
- [MySQL Connector/C++ APIs](https://dev.mysql.com/doc/dev/connector-cpp/latest/)
- [Connector/C++ X DevAPI Client Pool](https://dev.mysql.com/doc/dev/connector-cpp/latest/classmysqlx_1_1abi2_1_1r0_1_1Client.html)
