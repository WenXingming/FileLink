# 认证、会话与 Redis 缓存

FileLink 的权限模型可以浓缩为一句话：**浏览器持有一个随机会话令牌；MySQL 决定该令牌是否仍有效；Redis 只让这次查询更快。**

这条链路是上传、文件列表、下载、删除和分享管理的共同入口。业务服务接收到的不是“某个 Cookie 字符串”，而是已经由 `RequestAuthenticator` 解析出的 `AuthenticatedUser`。

## 一张完整的登录与鉴权图

```text
注册 / 登录
    │ 用户名 + 密码
    ▼
AuthService
    │ 1. Argon2id 哈希或校验密码
    │ 2. 生成 32 字节随机 token
    │ 3. BLAKE2b(token) 写入 MySQL user_sessions
    ▼
Set-Cookie: filelink_session=<原始 token 的十六进制文本>

后续受保护请求
    │ Cookie: filelink_session=...
    ▼
RequestAuthenticator ──► AuthService::current_user
                              │
                  Redis 命中？│是：得到 user_id，仍检查 MySQL users
                              │否或 Redis 故障
                              ▼
                MySQL：有效 session + 未禁用 user
                              │
                              ▼
                     AuthenticatedUser { user_id, username }
```

这不是 JWT：令牌本身不携带用户信息、过期时间或签名声明。服务端每次都能撤销它，因此称为**服务端会话**更准确。

## 先区分三种“哈希”


| 目的                    | 算法/实现                                   | 为什么                                       |
| ------------------------- | --------------------------------------------- | ---------------------------------------------- |
| 密码存储                | libsodium Argon2id                          | 故意慢且使用内存，抵抗离线猜密码             |
| 会话、分享 token 的索引 | libsodium`crypto_generichash`，默认 BLAKE2b | 固定 32 字节、快速、不可从哈希反推随机 token |
| 文件内容身份            | BLAKE3                                      | 适合流式计算和内容寻址去重                   |

不要说“密码和 token 都用同一种哈希”：它们面对的攻击不同。密码来自用户、熵可能很低，所以需要 Argon2id；会话 token 是服务端生成的 32 字节高熵随机数，快速哈希后存储即可避免数据库泄露时直接重放。

## 注册与登录如何建立会话

### 注册：创建账户和首个会话是一个事务

`POST /auth/register` 接收用户名和密码。用户名限 3–64 位 ASCII 字母、数字、`_`、`-`；密码限 8–128 字节。服务先确认用户名不存在，用 Argon2id 生成 `password_hash`，再生成 `user_id` 与会话：

```text
MySQL 单个事务：
  INSERT users(user_id, username, password_hash)
  INSERT user_sessions(token_hash, user_id, expires_at = 当前时间 + 7 天)
```

提交成功后才返回 `201 Created` 和 Cookie，因此不会出现“账户创建了但注册自动登录失败”的半完成状态。

### 登录：每次成功都创建一条新会话

`POST /auth/login` 先按用户名找到 User，拒绝被禁用账户，再用 Argon2id 校验密码。成功后插入新的 `user_sessions` 记录并返回 Cookie。

`user_sessions` 不以 `user_id` 为主键，所以一个用户可以在浏览器、手机和另一台电脑上同时拥有多条会话；每个 Cookie 对应其中一条由 `token_hash` 主键定位的记录。

## 当前登录状态查询 (/auth/me)

`GET /auth/me` 是一个用于客户端（如单页应用前端）查询当前登录状态与获取当前用户基本身份的接口：

* **会话提取与校验**：接口收到请求后，通过 [RequestAuthenticator](file:///home/wxm/FileLink/src/auth/RequestAuthenticator.h#L24) 尝试从 Cookie 请求头中解析并提取 `filelink_session` 令牌，然后利用 [AuthService](file:///home/wxm/FileLink/src/auth/AuthService.h#L73) 在 Redis 缓存或 MySQL 中验证该会话是否仍然处于活跃且未过期状态。
* **接口响应**：
  * 若会话验证成功且用户未被禁用，则返回 `200 OK` 并附带用户身份信息的 JSON（如 `{"username": "用户名称"}`）。
  * 若会话不存在、已过期、或所属用户已被禁用，则直接返回 `401 Unauthorized` 状态码（如 `{"message": "Unauthorized"}`）。
* **业务用途**：前端页面在加载或初始化时会首选调用该接口，以判断是否需要展示用户看板，或需要立即重定向到登录页面。

## Cookie、MySQL 和 Redis 分别保存什么


| 位置                 | 保存内容                                             | 生命周期                    | 安全/一致性职责          |
| ---------------------- | ------------------------------------------------------ | ----------------------------- | -------------------------- |
| 浏览器 Cookie        | 原始随机 token 的 64 位十六进制文本                  | `Max-Age=604800`，即 7 天   | 用户向服务端出示凭据     |
| MySQL`user_sessions` | `BLAKE2b(token)`、`user_id`、创建与过期时间          | 7 天，或被登出删除          | 会话有效性的唯一事实来源 |
| Redis                | `filelink:session:<token_hash hex> → <user_id hex>` | 与 MySQL 剩余有效期相同 TTL | 可随时重建的查询加速     |

认证响应设置的 Cookie 属性是：

```text
Path=/; HttpOnly; SameSite=Lax; Max-Age=604800
```

`HttpOnly` 阻止页面 JavaScript 读取 token，降低 XSS 直接窃取凭据的风险；`SameSite=Lax` 限制跨站请求自动携带 Cookie。生产环境经 TLS 部署时还应增加 `Secure`，确保浏览器只通过 HTTPS 发送 Cookie。

## 一次请求如何被认证

1. `RequestAuthenticator` 从 `Cookie` 请求头解析唯一的 `filelink_session`。缺失、为空或同名 Cookie 出现多次都按未认证处理。
2. `AuthService` 将 64 位十六进制文本解码回 32 字节原始 token，格式不合法直接拒绝。
3. 服务计算 BLAKE2b token 哈希，尝试读取 Redis。
4. Redis 命中时，得到缓存的 `user_id`，但仍查询 MySQL `users` 表并检查 `disabled_at`。用户被禁用时，缓存被删除并拒绝请求。
5. Redis 未命中、未启用或连接失败时，查询 MySQL `user_sessions` 中匹配且 `expires_at > NOW()` 的记录，再查询 User。成功后按“剩余会话秒数”写入 Redis。

注意：当前 Redis 命中路径不会再次查询 `user_sessions`，因为缓存 TTL 被设置为该会话的剩余时间，且正常登出会主动删除缓存项。这是“读会话记录”这一部分的性能优化；用户禁用状态依然每次从 MySQL 检查。

## 过期、登出与异常场景

### 自然过期

Cookie 的浏览器过期时间和 MySQL `expires_at` 都是 7 天。即使浏览器仍意外携带旧 Cookie，MySQL 查询中的 `expires_at > NOW()` 仍会拒绝它。Redis 的 TTL 不会长于剩余会话时间，因此不会将过期会话重新认证为有效。

### 登出

`POST /auth/logout` 从 Cookie 取 token，删除对应的 MySQL 会话行，并尝试删除 Redis 键；响应再用 `Max-Age=0` 覆盖浏览器 Cookie。没有 Cookie 或 token 已失效时，接口仍清除浏览器 Cookie，便于前端收口状态。

### Redis 宕机、超时或丢失数据

`RedisSessionCache` 将连接错误、超时和协议错误都视为 `Unavailable`，断开当前 hiredis 连接；`AuthService` 会自然走 MySQL 查询。Redis 丢数据只意味着更多 MySQL 查询，不会导致用户全部登出，也不会让无效 token 通过认证。

因此 Redis 是**可失效缓存**，而不是会话数据库。默认配置也关闭 Redis：`redis-enabled = false`。

## 三个常见追问

### 数据库泄露后，攻击者能直接登录吗？

不能直接使用 `user_sessions.token_hash` 登录。浏览器提交的是原始随机 token，不是哈希；服务会对提交值重新计算 BLAKE2b 后再比较。攻击者需要反推出 32 字节高熵随机 token，实际不可行。密码字段同样不是明文，而是 Argon2id 编码哈希。

### Redis 丢数据后，为什么仍能工作？

因为 Redis 键可以由下一次 MySQL 成功认证重新写入。MySQL 保留会话哈希、到期时间和用户关系，认证逻辑把 Redis 未命中与 Redis 不可用都视为“回查 MySQL”，并非“会话无效”。

### 缓存命中后，为什么仍检查用户禁用状态？

会话缓存只说明 token 最近对应某个 `user_id`，不说明账户当前仍可用。管理员若禁用用户，下一次请求必须立即失效；因此缓存命中仍按 `user_id` 查询 `users`，确认 `disabled_at` 为空。

## 面试时的讲述重点

“我使用服务端 Cookie 会话而不是把身份直接放在前端。密码用 Argon2id 存储；登录时服务端生成 32 字节随机 token，浏览器只通过 HttpOnly Cookie 持有原文，MySQL 只存 BLAKE2b 哈希和过期时间，所以数据库泄露不能直接重放会话。Redis 缓存 token 哈希到用户 ID 的映射，TTL 对齐数据库会话剩余时间；它不可用时回退 MySQL，缓存命中也仍检查账户是否被禁用，因此 Redis 只优化延迟，不参与权限真相。”

## 代码阅读路线

1. [`src/auth/AuthApiRouter.cpp`](../src/auth/AuthApiRouter.cpp)：注册、登录、登出和 Cookie 响应。
2. [`src/auth/RequestAuthenticator.cpp`](../src/auth/RequestAuthenticator.cpp)：Cookie 解析和受保护请求入口。
3. [`src/auth/AuthService.cpp`](../src/auth/AuthService.cpp)：密码校验、会话创建、MySQL 回退逻辑。
4. [`src/cache/RedisSessionCache.cpp`](../src/cache/RedisSessionCache.cpp)：键格式、TTL、连接错误处理。
5. [`src/database/UserSession.cpp`](../src/database/UserSession.cpp)：会话有效期的最终 SQL 条件。

读完后，你应该可以清楚区分“浏览器凭据”“持久化会话”和“缓存加速”，并解释为什么三者不能互相替代。
