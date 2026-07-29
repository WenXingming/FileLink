# Auth：认证模块设计与流程

本文对应当前 `src/auth/` 实现，目标是帮助读者从 HTTP 请求开始，理解注册、登录、会话鉴权和注销的完整流程。

## 一句话概括

FileLink 使用服务端 Session：浏览器保存原始随机令牌，MySQL 持久化令牌哈希和有效期，Redis 缓存令牌哈希到用户 ID 的映射，`AuthService` 负责协调这些状态。

## 模块地图

认证 HTTP 流程采用轻量的 MVC 分工，并在 Controller 前增加一个输入解析器：

```text
HttpRequest
    │
    ▼
AuthRequestParser   解析 JSON 和 Cookie，不决定 HTTP 响应
    │
    ▼
AuthApiRouter       Controller：编排一次 HTTP 请求
    │
    ▼
AuthService         Model / Application Service：执行认证业务
    │
    ▼
AuthResponseView    View：构造状态码、JSON 和 Set-Cookie
    │
    ▼
HttpResponse
```

这张图表达的是数据阅读顺序。实际控制关系由 Router 发起：Router 调用 Parser，随后调用 Service，最后选择一个 View 响应。


| 文件                                                   | 单一职责                                     |
| -------------------------------------------------------- | ---------------------------------------------- |
| [`AuthRequestParser`](../src/auth/AuthRequestParser.h) | 把`HttpRequest` 转换为用户名、密码或会话令牌 |
| [`AuthApiRouter`](../src/auth/AuthApiRouter.h)         | 注册认证端点并编排 Parser、Service 和 View   |
| [`AuthService`](../src/auth/AuthService.h)             | 编排用户、密码、会话、MySQL 和 Redis 业务    |
| [`AuthResponseView`](../src/auth/AuthResponseView.h)   | 把认证结果表示为`HttpResponse`               |
| [`PasswordHasher`](../src/auth/PasswordHasher.h)       | 封装 libsodium Argon2id 密码哈希             |

没有额外的认证器、接口、工厂或 DI 容器。`main.cpp` 创建一个具体的 `AuthService`，再把它注入需要认证能力的 Router。

## HTTP 接口


| 方法与路径            | 输入                      | 成功响应         | 作用                      |
| ----------------------- | --------------------------- | ------------------ | --------------------------- |
| `POST /auth/register` | JSON 用户名和密码         | `201 Created`    | 创建用户和初始会话        |
| `POST /auth/login`    | JSON 用户名和密码         | `200 OK`         | 校验密码并创建新会话      |
| `GET /auth/me`        | `filelink_session` Cookie | `200 OK`         | 查询当前登录用户          |
| `POST /auth/logout`   | 可选 Session Cookie       | `204 No Content` | 删除指定会话并清空 Cookie |

注册、登录和当前用户成功时，JSON 只返回公开信息：

```json
{"username":"alice"}
```

错误响应采用：

```json
{"message":"Invalid credentials"}
```

## 请求解析层：AuthRequestParser

[`AuthRequestParser.cpp`](../src/auth/AuthRequestParser.cpp) 只负责输入格式，不知道数据库、用户状态和 HTTP 状态码。

### 解析用户名和密码

`parse_credentials` 从请求 Body 解析 JSON，并读取两个字符串字段：

```json
{
  "username": "alice",
  "password": "correct-password"
}
```

JSON 无法解析、字段缺失或字段不是字符串时返回 `false`。Parser 不返回 `400`；由调用它的 Router 决定失败应映射为什么响应。

### 解析会话 Cookie

`parse_session_token` 通过 Tudou 的 `HttpRequest::get_header("Cookie")` 取得完整 Cookie Header，再提取唯一且非空的 `filelink_session`：

```http
Cookie: theme=dark; filelink_session=<64 位十六进制令牌>
```

缺少会话 Cookie、值为空或出现多个同名 Cookie 时返回 `false`。这个函数也被 File、Share 和 Upload Router 复用，因此会话 Cookie 的格式只在一处维护。

Parser 不验证 64 位文本是否合法，也不查询会话；这些属于 `AuthService`。

## Controller：AuthApiRouter

[`AuthApiRouter.cpp`](../src/auth/AuthApiRouter.cpp) 的每个 handler 都保持相同的平铺结构：

```text
1. Parser 解析外部输入
2. AuthService 执行业务
3. switch 映射领域结果
4. AuthResponseView 构造响应
```

Router 知道 HTTP 端点和状态映射，但不知道 Cookie 如何拆分、JSON 如何序列化、密码如何哈希或 SQL 如何执行。

例如 `/auth/me` 的主流程是：

```text
解析 filelink_session
    │失败
    └──────────────► 401 Unauthorized
    │成功
    ▼
AuthService::current_user
    ├── Success        ─► 200 + username
    ├── InvalidSession ─► 401 Unauthorized
    └── SystemError    ─► 500 Internal Server Error
```

## View：AuthResponseView

[`AuthResponseView.cpp`](../src/auth/AuthResponseView.cpp) 集中维护认证接口的输出格式：

- `registered`：`201 Created`，返回用户名并设置会话 Cookie。
- `logged_in`：`200 OK`，返回用户名并设置会话 Cookie。
- `current_user`：`200 OK`，只返回用户名。
- `logged_out`：`204 No Content`，把会话 Cookie 的 `Max-Age` 设为 `0`。
- `error`：构造统一的 JSON 错误对象。

注册或登录成功时设置：

```http
Set-Cookie: filelink_session=<token>; Path=/; HttpOnly; SameSite=Lax; Max-Age=604800
```

`AuthResponseView` 不读取 `HttpRequest`，也不调用 `AuthService`。Router 选择响应种类，View 只负责正确构造它。

## Service：AuthService

[`AuthService.cpp`](../src/auth/AuthService.cpp) 是认证业务编排入口。它依赖具体的 MySQL 连接池和可选的 `UserSessionCache`，不依赖 Tudou 的请求或响应类型。

### 注册

```text
校验用户名和密码
    ▼
检查用户名是否已存在
    ▼
生成 16 字节 user_id
    ▼
Argon2id 哈希密码
    ▼
事务开始
    ├── INSERT users
    └── INSERT user_sessions
    ▼
事务提交
```

当前用户名规则为 3～64 个字符，只允许字母、数字、下划线和连字符；密码长度为 8～128 字节。

用户和初始会话在同一个数据库事务中创建，保证二者同时成功或同时回滚。

### 登录

```text
按用户名查询用户
    ▼
确认用户未禁用
    ▼
PasswordHasher::verify 校验密码
    ▼
创建一条新的 user_sessions 记录
```

每次成功登录都会创建独立会话，不会覆盖该用户已有的其他会话。因此同一用户可以同时拥有多个登录会话，注销时只删除客户端提交的那个令牌。

### 创建会话

`create_session` 完成三个动作：

1. 使用 libsodium 生成 32 字节随机令牌。
2. 计算令牌的 32 字节通用哈希并写入 MySQL。
3. 把原始令牌编码为 64 位十六进制文本，交给 View 写入 Cookie。

原始令牌不会写入 MySQL。数据库只保存无法直接作为 Cookie 重放的 `token_hash`。

会话有效期在创建时固定为 7 天。后续访问不会延长数据库的 `expires_at`，也不会重新发送 Session Cookie，因此当前实现不是滑动过期。

## 当前用户鉴权流程

`AuthService::current_user` 接收 Parser 已提取的十六进制令牌：

```text
解码 64 位十六进制 token
    │失败
    └──────────────► InvalidSession
    │成功
    ▼
计算 token_hash
    ▼
查询 Redis：token_hash → user_id
    ├── Hit ─────────► 使用缓存中的 user_id
    └── Miss/Unavailable
                       ▼
                MySQL 查询未过期 user_sessions
                       │不存在或已过期
                       └────────► InvalidSession
                       │有效
                       ▼
                取得 user_id 并回填 Redis
    ▼
MySQL 查询 users，并确认用户未禁用
    ▼
返回 AuthenticatedUser { user_id, username }
```

这里需要准确区分两种状态：

- MySQL `user_sessions` 是会话的持久化记录。
- Redis 是带 TTL 的会话定位缓存；命中时直接提供 `user_id`，不再查询 `user_sessions`，但仍然查询 MySQL `users` 确认用户存在且未禁用。

Redis 未启用、连接失败、键不存在或值不可解析时，都不会直接让用户登出，而是统一回退到 MySQL 查询有效会话。缓存回填 TTL 使用 MySQL 会话的剩余有效秒数，不会超过数据库过期时间。

## 注销流程

注销被设计为幂等操作：客户端没有 Cookie、令牌格式错误或数据库中已经没有该会话时，都返回 `204 No Content`。

对于可解码的令牌，`AuthService::logout`：

1. 计算 `token_hash`。
2. 从 MySQL `user_sessions` 删除对应记录。
3. 删除 Redis 中对应缓存键。
4. View 下发 `Max-Age=0` 清空浏览器 Cookie。

只有数据库操作抛出异常时，Service 才返回失败，Router 将其映射为 `500`。

## MySQL 与 Redis 的数据分工


| 位置                 | 保存内容                                             | 生命周期                 | 作用                     |
| ---------------------- | ------------------------------------------------------ | -------------------------- | -------------------------- |
| 浏览器 Cookie        | 原始令牌的 64 位十六进制文本                         | 7 天或注销清除           | 客户端提交的会话凭证     |
| MySQL`user_sessions` | 32 字节`token_hash`、16 字节 `user_id`、过期时间     | 7 天、主动注销或定时清理 | 会话持久化记录           |
| Redis                | `filelink:session:<token_hash_hex> → <user_id_hex>` | MySQL 剩余有效时间       | 加速令牌到用户 ID 的定位 |

[`UserSessionDao`](../src/database/UserSession.h) 只负责创建、查询未过期会话和删除会话。有效性查询使用 `expires_at > NOW()`。

[`UserSessionCache`](../src/redis/UserSessionCache.h) 把 Redis 查询结果区分为：

- `Hit`：找到并成功解析用户 ID。
- `Miss`：键不存在或缓存值不可解析。
- `Unavailable`：Redis 未启用、连接失败或输入不合法。

MySQL migration 还创建了每日执行的 `cleanup_expired_user_sessions` 事件，用于物理删除已经过期的会话行。在线鉴权不依赖清理事件及时执行，因为 DAO 查询本身会排除过期记录。

## 三种哈希不要混淆


| 用途         | 算法                                     | 原因                                                       |
| -------------- | ------------------------------------------ | ------------------------------------------------------------ |
| 密码存储     | Argon2id                                 | 密码熵较低，需要慢速、内存困难的密码哈希抵抗离线猜测       |
| 会话令牌索引 | libsodium`crypto_generichash`（BLAKE2b） | 令牌由服务端随机生成，哈希用于持久化索引和避免保存原始凭证 |
| 文件内容寻址 | BLAKE3                                   | 面向大文件流式计算、去重和物理对象定位                     |

密码哈希由 [`PasswordHasher.cpp`](../src/auth/PasswordHasher.cpp) 独立封装，因此 `AuthService` 不需要知道 Argon2id 参数和 libsodium C API 细节。

## 组合根与跨模块复用

[`main.cpp`](../src/main.cpp) 是组合根：

```text
MySQL connection_pool ─┐
                      ├── AuthService
UserSessionCache ─────┘
                           │
         ┌─────────────────┼─────────────────┐
         ▼                 ▼                 ▼
 AuthApiRouter       File/Share Router   UploadApiRouter
```

`AuthApiRouter` 使用完整的 Parser → Service → View 流程。File、Share 和 Upload Router 复用 `AuthRequestParser::parse_session_token` 与 `AuthService::current_user`，但保留各自协议需要的响应格式，例如 Upload Router 使用 Tus 错误响应。

## 结果类型

Service 不返回 HTTP 状态码，而是返回领域结果：

- `RegisterResult`：成功、用户名非法、密码非法、用户名已存在、系统错误。
- `LoginResult`：成功、凭据无效、系统错误。
- `CurrentUserResult`：成功、会话无效、系统错误。
- `logout`：布尔值；幂等的无效会话仍算成功，系统异常才算失败。

这样 `AuthService` 可以独立于 HTTP 使用，而不同 Router 可以根据自己的协议映射响应。

## 测试地图


| 测试                                                            | 覆盖内容                                     |
| ----------------------------------------------------------------- | ---------------------------------------------- |
| [`PasswordHasherTests.cpp`](../tests/PasswordHasherTests.cpp)   | 密码哈希、正确密码、错误密码和损坏哈希       |
| [`AuthServiceTest.cpp`](../tests/AuthServiceTest.cpp)           | 注册、登录、会话、禁用用户、Redis 回退和注销 |
| [`AuthApiTest.cpp`](../tests/AuthApiTest.cpp)                   | 四个认证端点、JSON、状态码和 Cookie          |
| [`UserSessionCacheTest.cpp`](../tests/UserSessionCacheTest.cpp) | Redis 会话映射的写入、读取和删除             |
| File、Share、Tus API 测试                                       | 受保护业务接口对认证模块的复用               |

## 推荐代码阅读顺序

1. [`AuthApiRouter.cpp`](../src/auth/AuthApiRouter.cpp)：先看四个接口的顶层业务流程。
2. [`AuthRequestParser.cpp`](../src/auth/AuthRequestParser.cpp)：理解外部 HTTP 输入如何变成 Service 参数。
3. [`AuthResponseView.cpp`](../src/auth/AuthResponseView.cpp)：理解状态码、JSON 和 Cookie 如何输出。
4. [`AuthService.cpp`](../src/auth/AuthService.cpp)：理解注册、登录、鉴权与注销业务。
5. [`PasswordHasher.cpp`](../src/auth/PasswordHasher.cpp)：理解密码哈希边界。
6. [`UserSession.cpp`](../src/database/UserSession.cpp)：理解会话 SQL。
7. [`UserSessionCache.cpp`](../src/redis/UserSessionCache.cpp)：理解 Redis 缓存与故障回退。

阅读时始终沿着这条主线：

```text
外部 HTTP 输入 → Parser → Router → Service → Router → View → HTTP 输出
```
