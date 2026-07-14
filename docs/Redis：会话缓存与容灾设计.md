# Redis：会话缓存与容灾设计

在 FileLink 项目中，[src/redis/](file:///home/wxm/FileLink/src/redis/) 模块主要负责提供高性能的登录会话二级缓存。它承接了 **Web 服务器高频鉴权请求的读压力**，并将元数据一致性保障安全地交由 MySQL 后端处理。本篇文档将详细介绍 Redis 模块的接口设计、存储结构、生命周期控制及故障退避容灾机制。

---

## 模块职责与核心类

Redis 模块由一个配置结构体与一个缓存核心操作类组成，其职责分工如下：

* **运行时配置（[RedisConfig.h](file:///home/wxm/FileLink/src/redis/RedisConfig.h)）**：由 [RedisConfig](file:///home/wxm/FileLink/src/redis/RedisConfig.h#L12) 结构体定义，包含是否启用缓存、主机地址、端口以及连接和套接字操作的超时时间（`timeoutMilliseconds`）。
* **缓存管理器（[UserSessionCache.h](file:///home/wxm/FileLink/src/redis/UserSessionCache.h) / [UserSessionCache.cpp](file:///home/wxm/FileLink/src/redis/UserSessionCache.cpp)）**：由 [UserSessionCache](file:///home/wxm/FileLink/src/redis/UserSessionCache.h#L24) 类实现。它使用官方 C 语言客户端 `hiredis` 建立与 Redis 容器的连接，通过内部互斥锁 `std::mutex` 保证多线程并发安全，并对外提供会话的查询、存储和显式移除接口。

---

## 存储结构与数据映射

为了在缓存中获得良好的文本兼容性并保证二进制安全性，会话数据在写入 Redis 之前会进行编码转换。其存储结构分配如下表所示：


| 属性             | 格式与内容                          | 字节开销估算 | 设计职责                                                    |
| :----------------- | :------------------------------------ | :------------- | :------------------------------------------------------------ |
| **键 (Key)**     | `filelink:session:<token_hash_hex>` | 81 字节      | 会话的唯一标识，其中包含 32 字节 Token 哈希的十六进制编码。 |
| **值 (Value)**   | 用户 UUID 的 32 位十六进制编码      | 32 字节      | 标识会话所属的物理用户，由 16 字节原始`user_id` 转换而来。  |
| **存活期 (TTL)** | 动态对齐 MySQL 剩余生存秒数         | -            | 决定该记录在内存中的最高保留时间。                          |

在底层通信时，[UserSessionCache](file:///home/wxm/FileLink/src/redis/UserSessionCache.h#L24) 统一使用 `hiredis` 的 `%b` 二进制安全占位符来发送 Key，避免了 Token 哈希可能含有的 `\0` 等字符导致 SQL 截断或命令解析错误。

---

## 生命周期与过期控制

会话在缓存中的生命周期与 MySQL 保持强一致，采用**绝对过期时间**策略，并在缓存读取命中时**不刷新** TTL：

```text
 用户登录成功
     │ 写入 MySQL，设定过期时间为 T = 登录时间 + 7 天
     ▼
 首次发起请求 -> 缓存未命中 -> 查询 MySQL
     │ 计算剩余秒数：TTL = T - 当前时间
     │ 执行 SET KEY VALUE EX TTL 写入 Redis
     ▼
 续传或后续请求 -> 缓存命中 (只读 GET)
     │ 正常提取数据，Redis 的 TTL 持续倒计时 (不因 GET 而重置或刷新)
     ▼
 达到 7 天期限
     │ Redis 原生过期自动清理（惰性删除 + 后台主动扫描）
     │ MySQL 过期行由事件调度器每日就地清除
```

这种固定的存活期设计，强制将会话被窃取后的最大暴露窗口限制在 7 天内，并使得 Redis 中的会话生命期存在确定性的上限。依靠 Redis 原生的过期机制，键一旦到期会被自动彻底抹除，单条会话仅消耗约 500 字节物理内存，这从根本上杜绝了缓存垃圾数据无限堆积和内存加剧的问题。

---

## 故障退避与主备配合

Redis 在架构中扮演的是**非强依赖的查询加速层**，系统设计了完善的退避容灾机制以保证核心鉴权的高可用：

```text
 鉴权请求到达
      │
      ▼
 UserSessionCache::find_user_id
      │
      ├─► [ 正常状态 ] ──► 尝试读取 Redis ──► 命中返回
      │                                       │ (缓存未命中或不可用)
      │                                       ▼
      └─► [ Redis 故障 / 禁用 ] ──────────► 降级回退
                                              │
                                              ▼
                                       直接查询 MySQL 
                                 (有效会话检验 + 软禁用状态)
```

在网络连接中断、命令超时或 Redis 服务意外宕机时，`UserSessionCache` 会捕获异常，通过 `reset_connection_locked` 物理释放死连接，并向业务层返回 `CacheLookupResult::Unavailable` 状态码。此时，认证服务层会无感地降级为直接向 MySQL 关系数据库发送查询。Redis 宕机或丢数据只会短暂增加 MySQL 的读取负担，绝不会导致在线用户被强制登出，也绝不会让无效的假令牌通过校验，实现了极佳的系统弹性。
