# 数据访问层与 SOCI 集成

在 FileLink 项目中，**数据访问层（DAL）负责持久化业务元数据，并作为所有并发控制和物理对象生命周期的唯一事实来源**。我们**使用 [SOCI](https://soci.sourceforge.net/) 作为底层的 C++ 数据库访问库**。

本篇文档将详细介绍 `src/database` 模块的技术选型背景、底层架构设计、核心实现模式以及开发最佳实践。

---

## 为什么选择 SOCI

FileLink 选择 **SOCI + libmysqlclient**，主要基于避免重复造轮子、保证资源生命周期安全和易用性的考量。在 C++ 中连接 MySQL 时，我们评估了三种主流选择：直接使用官方 `libmysqlclient` C API、官方 `Connector/C++`，或使用 `SOCI` 访问层。

直接使用**原始的 `libmysqlclient` C API** 会使项目面临自行维护底层逻辑的负担。这包括编写繁琐的参数绑定结构体、手动转换类型，以及实现线程安全的连接池。更重要的是，C 风格的错误处理需要在每个分支手动释放资源，在复杂的异常路径上极易引发连接泄露或事务未完全回滚。

官方 `Connector/C++` 的 X DevAPI 虽然自带连接池，但它必须引入 X Protocol 和额外的通信端口，这超出了项目仅需传统关系型 SQL 的诉求；而它的 Classic JDBC 接口虽然支持普通 SQL，却又**不包含内置连接池**，仍需应用层自行组织并管理连接的复用与生命周期。

SOCI 恰好补齐了上述所有的痛点。作为官方驱动之上的轻量级 C++ 包装层，它原生支持 **C++ 风格的参数绑定**以防范 SQL 注入，**自带成熟的连接池**和租借状态管理，并提供 **RAII 形式的事务生命周期管理**。同时，**底层仍基于官方驱动，保证了网络通信与协议解析的高可靠性**。

## 分层架构

本项目的数据库访问在逻辑上分为四层，通过接口与具体的服务解耦：

```text
UploadService / FileService
     │
     ▼
  UserDao / FileDao / ObjectDao  (DAO 数据访问对象)
     │
     ▼
   SOCI (C++ 访问层封装，管理 Connection Pool)
     │
     ▼
 libmysqlclient (MySQL 官方客户端库)
     │
     ▼
   MySQL Server (关系型数据库，监听 3306)
```

 `libmysqlclient` 仍然是实际连接 MySQL 的官方客户端；SOCI 是其上的轻量 C++ 访问层。。

## 目录结构与模块划分

数据访问层位于 [src/database/](file:///home/wxm/FileLink/src/database/) 目录下。其文件划分严格遵循“一个实体 + 一个 DAO”的原则，使得业务模型与底层存储紧密结合：


| 文件名称                                                                                                                                     | 对应数据库表      | 核心职责                                                                                                                        |
| :--------------------------------------------------------------------------------------------------------------------------------------------- | :------------------ | :-------------------------------------------------------------------------------------------------------------------------------- |
| [SociSessionLease.h](file:///home/wxm/FileLink/src/database/SociSessionLease.h)                                                              | -                 | 提供基于 RAII 的`soci::connection_pool` 连接会话租借和释放机制。                                                                |
| [MySqlConfig.h](file:///home/wxm/FileLink/src/database/MySqlConfig.h)                                                                        | -                 | 定义连接池容量、超时和地址等参数的[MySqlConfig](file:///home/wxm/FileLink/src/database/MySqlConfig.h#L12) 结构体。              |
| [User.h](file:///home/wxm/FileLink/src/database/User.h) / [.cpp](file:///home/wxm/FileLink/src/database/User.cpp)                            | `users`           | 账户认证实体与[UserDao](file:///home/wxm/FileLink/src/database/User.h#L23)，处理用户注册、查询与禁用状态。                      |
| [UserSession.h](file:///home/wxm/FileLink/src/database/UserSession.h) / [.cpp](file:///home/wxm/FileLink/src/database/UserSession.cpp)       | `user_sessions`   | 登录会话实体与[UserSessionDao](file:///home/wxm/FileLink/src/database/UserSession.h#L23)，执行 Session Token 的存取与过期校验。 |
| [File.h](file:///home/wxm/FileLink/src/database/File.h) / [.cpp](file:///home/wxm/FileLink/src/database/File.cpp)                            | `files`           | 逻辑文件实体与[FileDao](file:///home/wxm/FileLink/src/database/File.h#L26)，表达文件所有权、展示名称与对物理对象的引用。        |
| [Object.h](file:///home/wxm/FileLink/src/database/Object.h) / [.cpp](file:///home/wxm/FileLink/src/database/Object.cpp)                      | `objects`         | 物理对象实体与[ObjectDao](file:///home/wxm/FileLink/src/database/Object.h#L33)，维护磁盘文件的引用计数和垃圾回收状态。          |
| [Share.h](file:///home/wxm/FileLink/src/database/Share.h) / [.cpp](file:///home/wxm/FileLink/src/database/Share.cpp)                         | `shares`          | 外链分享实体与`ShareDao`，提供分享口令哈希、有效期及撤销标志管理。                                                              |
| [UploadSession.h](file:///home/wxm/FileLink/src/database/UploadSession.h) / [.cpp](file:///home/wxm/FileLink/src/database/UploadSession.cpp) | `upload_sessions` | 分片上传实体与[UploadSessionDao](file:///home/wxm/FileLink/src/database/UploadSession.h#L38)，追踪断点续传的阶段偏移量。        |

---

## 核心设计模式

### RAII 连接生命周期

由于底层采用多线程 I/O 模型，各个线程需要并发地获取和释放连接。为了绝对避免连接因早期返回或异常抛出而泄露，项目设计了 [SociSessionLease](file:///home/wxm/FileLink/src/database/SociSessionLease.h#L13) 辅助类。

[SociSessionLease](file:///home/wxm/FileLink/src/database/SociSessionLease.h#L13) 的构造函数负责从连接池中租借连接，析构函数则在离开作用域时自动将连接归还给连接池。为了保证连接排他性，该类禁用了拷贝构造与赋值运算符：

```cpp
class SociSessionLease {
public:
    explicit SociSessionLease(soci::connection_pool& pool)
        : pool_(pool), position_(pool.lease()) {}
    ~SociSessionLease() { pool_.give_back(position_); }

    soci::session& get() { return pool_.at(position_); }
};
```

### 依赖注入模式

DAO 类本身不管理连接池，也不自行建立连接。它们统一在构造函数中通过引用接收外部传入的 `soci::session&`。这种设计使得业务层能够以声明式的方式控制事务边界。在同一业务函数中，多个不同的 DAO 可以安全地复用同一个 `soci::session` 实例，从而确保所有 SQL 操作都在同一个事务或连接上下文中运行。同时，这也极大简化了单元测试的编写。

---

## SOCI 编码技术实践

在实现具体的 DAO 查询时，项目采用了多项 SOCI 的进阶特性来确保类型安全与防范 SQL 注入。

### 参数绑定与类型转换

为防范 SQL 注入，项目严禁拼接 SQL 字符串。所有的输入变量统一通过 `soci::use` 进行参数绑定：

```cpp
sql_ << "INSERT INTO users (user_id, username, password_hash) "
        "VALUES (:id, :username, :password_hash)",
        soci::use(user.user_id),
        soci::use(user.username),
        soci::use(user.password_hash);
```

对于查询结果的装载，则通过 `soci::into` 映射到具体的 C++ 变量或对象属性：

```cpp
sql_ << "SELECT username FROM users WHERE user_id = :id",
        soci::into(out_username),
        soci::use(user_id);
```

### 处理 SQL 中的 NULL 值

MySQL 中的部分字段（如禁用时间 `disabled_at`、分享表的撤销时间 `revoked_at`）允许为 `NULL`。SOCI 通过引入 `soci::indicator` 变量来指示字段是否为空。在执行查询时，如果 indicator 的状态为 `soci::i_null`，说明字段为空；若为 `soci::i_ok`，则表示字段含有有效值。

```cpp
soci::indicator disabled_ind;
std::tm disabled_at{};

sql_ << "SELECT disabled_at FROM users WHERE user_id = :id",
        soci::into(disabled_at, disabled_ind),
        soci::use(user_id);

if (disabled_ind == soci::i_ok) {
    // 字段有值，用户已被禁用
} else if (disabled_ind == soci::i_null) {
    // 字段为 NULL，用户正常
}
```

### 常用字段类型的映射关系

在类型转换方面，MySQL 中用于高效率存储 UUID 和哈希值的 `BINARY(16)` 和 `BINARY(32)` 类型，在 C++ 代码中均映射为包含原始二进制字节流的 `std::string`。对于日期和时间字段（如 `DATETIME` 或 `TIMESTAMP`），SOCI 原生支持与 C++ 标准库中的 `std::tm` 结构体进行相互映射，避免了手动解析时间字符串的麻烦。

---

## 事务管理与异常安全

当需要执行跨多表的复杂元数据修改时，我们必须依靠数据库事务。SOCI 提供了基于 RAII 机制的事务控制类 `soci::transaction`。

如果在执行过程中没有调用 `transaction.commit()` 并且程序因 `throw` 异常或 `return false` 提前退出了该函数作用域，`transaction` 的析构函数会自动调用 `rollback()`，从而保护了数据库的一致性。以下是删除逻辑文件并安全扣减物理对象引用的典型写法：

```cpp
bool FileService::delete_file(const std::string& owner_user_id, const std::string& file_id) {
    db::SociSessionLease lease(pool_);
    soci::session& sql = lease.get();
  
    // 开启事务 (RAII)
    soci::transaction transaction(sql);

    db::File file;
    db::FileDao file_dao(sql);
    if (!file_dao.find_by_id_and_owner(file_id, owner_user_id, file)) {
        return false;
    }
  
    // 1. 删除逻辑文件
    if (!file_dao.remove_by_id_and_owner(file_id, owner_user_id)) {
        return false;
    }
  
    // 2. 递减物理对象的引用计数
    if (!db::ObjectDao(sql).remove_reference(file.content_hash)) {
        throw std::runtime_error("logical file references a missing object");
    }

    // 显式提交事务
    transaction.commit();
    return true;
}
```

---

## 开发与扩展建议

如果您需要添加新的业务实体或修改现有表结构，请遵循以下开发步骤：

* **编写 SQL 迁移脚本**：在 `database/migrations/` 中编写有序的 `.sql` 脚本以更新 MySQL 表定义。
* **定义数据结构**：在 `src/database/` 下创建新的模型 `struct`，并尽量将数据库可能为空的字段在 C++ 中设计为布尔标识或配合 `soci::indicator` 映射。
* **编写 DAO 类**：继承 `soci::session&` 的持有结构，在 DAO 的具体方法中实现 SQL 的绑定和结果转换。
* **集成集成测试**：在 `tests/` 目录下，加上以 `integration` 标签结尾的 GoogleTest 集成测试，运行实际的 MySQL 检验 DAO 的 SQL 语句是否语法正确且索引高效。
