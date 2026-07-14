# 认证、会话与 Redis 缓存

FileLink 的权限校验机制可以概括为：浏览器持有高熵随机会话令牌，MySQL 存储令牌哈希以决定有效性，而 Redis 则作为查询加速缓存来降低延迟。整个业务系统的受保护接口均受此链路的保护，它们接收到的并非原始的 Cookie 字符串，而是经由拦截层验证并解析出的用户身份上下文。

---

## 登录与鉴权流程

在注册或登录成功后，系统会通过底层加密库生成一个 32 字节的随机高熵会话令牌，并通过 `Set-Cookie` 将其写回客户端浏览器。为了防范数据库泄露的潜在安全风险，MySQL 并不直接保存原始的令牌字符，而是仅保存其经由 BLAKE2b 算法计算后的单向加密哈希值。

当后续请求携带 Cookie 访问受保护接口时，请求拦截器会先计算令牌的哈希，并优先在 Redis 中检索对应的用户 ID。一旦发生缓存未命中或 Redis 暂时不可用，服务会自动回退到 MySQL 中查询，验证会话是否处于有效期内。如果会话合法，还会顺便检查对应用户是否被管理员软禁用，最后将验证结果存回 Redis 中以加速后续的鉴权访问。这整个登录与鉴权的生命周期流动如下图所示：

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

---

## 身份验证与缓存控制代码实现

身份认证逻辑主要由两层组成：[RequestAuthenticator](file:///home/wxm/FileLink/src/auth/RequestAuthenticator.h#L24)（HTTP 拦截适配层）和 [AuthService](file:///home/wxm/FileLink/src/auth/AuthService.h#L73)（核心鉴权业务层）。

### 1. HTTP 拦截校验的实现

拦截层解析 HTTP 报头中的 `Cookie` 字段，提取 `filelink_session` 令牌，若合法则调用底层服务：

```cpp
// src/auth/RequestAuthenticator.cpp 中核心的识别与派发
RequestAuthResult RequestAuthenticator::authenticate(const HttpRequest& request,
    AuthenticatedUser& out_user) const {
    std::string session_token;
    // 提取并校验唯一的 Cookie 令牌
    if (!this->session_token(request, session_token)) {
        return RequestAuthResult::Unauthorized;
    }

    // 委派给核心服务层校验
    switch (auth_service_.current_user(session_token, out_user)) {
    case CurrentUserResult::Success:
        return RequestAuthResult::Authenticated;
    case CurrentUserResult::InvalidSession:
        return RequestAuthResult::Unauthorized;
    case CurrentUserResult::SystemError:
        return RequestAuthResult::SystemError;
    }
    return RequestAuthResult::SystemError;
}
```

### 2. 缓存路由与回退的实现

在服务层中，校验首先尝试在 Redis 缓存中查询。如果缓存未命中或不可用，系统无缝回退到 MySQL，并在成功后回填缓存：

```cpp
// src/auth/AuthService.cpp 中 current_user 核心校验流程
CurrentUserResult AuthService::current_user(const std::string& session_token,
    AuthenticatedUser& out_user) {
    if (sodium_init() < 0) return CurrentUserResult::SystemError;

    std::string token;
    if (!decode_token(session_token, token)) return CurrentUserResult::InvalidSession;

    const std::string token_hash = hash_token(token);
    std::string user_id;

    // 1. 尝试使用二级缓存快速查询
    if (session_cache_ != nullptr
        && session_cache_->find_user_id(token_hash, user_id) == redis::CacheLookupResult::Hit) {
        try {
            db::SociSessionLease lease(pool_);
            db::User user;
            // 缓存命中也必须单表快速校验用户的软禁用状态，确保恶意账号可实时封禁
            if (!db::UserDao(lease.get()).find_by_id(user_id, user) || user.is_disabled) {
                session_cache_->remove(token_hash); // 强制清除脏缓存
                return CurrentUserResult::InvalidSession;
            }
            out_user = {user.user_id, user.username};
            return CurrentUserResult::Success;
        } catch (const std::exception&) {
            return CurrentUserResult::SystemError;
        }
    }

    // 2. 缓存未命中或不可用时的 MySQL 降级验证
    try {
        db::SociSessionLease lease(pool_);
        soci::session& sql = lease.get();
        db::UserSession session;
        // 在 MySQL 中验证会话有效性 (expires_at > NOW())
        if (!db::UserSessionDao(sql).find_active(token_hash, session)) {
            return CurrentUserResult::InvalidSession;
        }

        db::User user;
        if (!db::UserDao(sql).find_by_id(session.user_id, user) || user.is_disabled) {
            return CurrentUserResult::InvalidSession;
        }

        out_user = {user.user_id, user.username};
        // 3. 库查询成功，以该会话在 MySQL 中的剩余有效秒数作为 TTL 写入 Redis 缓存
        if (session_cache_ != nullptr) {
            session_cache_->store_user_id(token_hash, user.user_id,
                remaining_session_seconds(session.expires_at));
        }
        return CurrentUserResult::Success;
    } catch (const std::exception&) {
        return CurrentUserResult::SystemError;
    }
}
```

---

## 当前登录状态查询 (/auth/me)

单页应用前端在初始化或刷新页面时，会发起 `GET /auth/me` 请求来校验当前的登录状态。该接口通过 [RequestAuthenticator](file:///home/wxm/FileLink/src/auth/RequestAuthenticator.h#L24) 尝试从 Cookie 请求头中解析并提取会话令牌，并利用 [AuthService](file:///home/wxm/FileLink/src/auth/AuthService.h#L73) 进行有效性校验。

若校验通过且用户处于正常状态，接口将返回 `200 OK` 并附带当前用户的用户名，引导前端展示正常的用户管理界面；若校验失败（如会话过期或不存在），接口会统一返回 `401 Unauthorized` 错误，提示前端路由将用户重定向至登录页面。这使得前端应用可以低成本地在冷启动时确认当前用户的认证状态。

---

## 三种哈希的用途区分

为了应对不同的安全和性能要求，项目中同时采用了三种不同的哈希算法。它们分别面对不同的攻击模型，在保证强度的同时兼顾系统吞吐：

* **密码存储哈希 (Argon2id)**：密码由用户提供，容易因字符组合简单而遭受暴力猜解。因此系统使用 libsodium 提供的 Argon2id 算法（`crypto_pwhash_ALG_ARGON2ID13`），通过故意设计的慢速和高内存开销来抵抗离线暴破。
* **会话与分享令牌索引 (BLAKE2b)**：会话令牌是服务端生成的 32 字节高熵随机数，面临的主要威胁是数据库泄露后的直接重放。因此系统使用 libsodium 的通用哈希接口计算其 BLAKE2b 值（`crypto_generichash`）进行存储，在保证极高吞吐量的同时确保无法被逆向推导。
* **文件内容唯一标识 (BLAKE3)**：用于物理文件在磁盘上的内容寻址和去重，要求哈希必须支持极快地对大文件流式计算，因此选择 BLAKE3 算法。

---

## 凭证、数据库与缓存的职责分工

会话的默认有效期统一设置为 7 天。在整个生命周期中，Cookie、数据库与缓存各自承担着不同的职责：


| 存储位置             | 保存内容                                             | 生命周期                      | 安全与一致性职责             |
| :--------------------- | :----------------------------------------------------- | :------------------------------ | :----------------------------- |
| 浏览器 Cookie        | 原始随机 token 的 64 位十六进制文本                  | `Max-Age=604800`，即 7 天     | 用户请求时出示的安全凭据     |
| MySQL`user_sessions` | `BLAKE2b(token)`、`user_id`、创建与过期时间          | 7 天，或被主动登出时删除      | 会话有效性的**唯一真相来源** |
| Redis 缓存           | `filelink:session:<token_hash_hex> → <user_id_hex>` | 与 MySQL 剩余有效期相同的 TTL | 可随时重建的查询加速缓存     |

为了防止跨站脚本攻击与请求伪造，认证响应设置的 Cookie 属性为 `Path=/; HttpOnly; SameSite=Lax; Max-Age=604800`。其中 `HttpOnly` 阻止页面 JavaScript 读取 token，降低 XSS 窃取凭据的风险；`SameSite=Lax` 则限制跨站请求自动携带 Cookie。

---

## 会话存储与过期清理机制

对于用户主动点击登出的行为，系统会立即从 MySQL 物理删除该会话记录，并同步清除对应的 Redis 缓存键。对于用户直接关闭浏览器导致会话自然过期的场景，项目采用了数据库端的**事件调度器（Event Scheduler）**进行兜底清理。通过在 MySQL 中部署定时运行的后台事件，数据库每天会自动清除所有 `expires_at` 早于当前时间的过期行。这种就地清理的机制既避免了过期行在数据库中的无限堆积与索引膨胀，又无需向 C++ 业务层引入额外的定时扫描逻辑。

## 固定生命周期与滑动过期设计

目前系统对用户会话采用的是固定生命周期策略（即非滑动窗口过期）。会话的过期截止时间自用户最后一次成功登录或注册的时刻起，就被硬性锁定为 7 天。在后续的用户活跃访问（包括 `/auth/me` 状态校验）中，系统仅对令牌执行只读校验，并不会去更新数据库中的 `expires_at` 字段，也不会在 HTTP 响应中重新发送 `Set-Cookie` 响应头。这意味着，即便用户在这 7 天内每天都在持续使用系统，到期后其会话仍会强行失效，前端会因收到 401 响应而引导用户重新登录。

这种固定生存期设计能强制性地将任何泄露令牌的最大风险窗口限制在 7 天以内，并且避免了每次身份鉴权请求都伴随数据库写更新操作带来的性能开销。如果未来需要支持自动延期的滑动过期策略，可以在 `AuthService::current_user` 校验时检测令牌的剩余有效期（例如当剩余时间不足 3 天时），执行一次 MySQL 及 Redis 的过期时间更新，并在响应头中重新下发 `Set-Cookie` 以顺延浏览器的 Cookie 生命周期。

---

## Redis 故障容灾与一致性设计

Redis 在这里被严格定位为可随时重建的缓存加速器，而不是记录有效性真相的会话数据库。当 Redis 发生网络超时、断连或意外的数据丢失时，系统会将所有的认证请求安全地降级回退到 MySQL 关系库上进行判定，因此不会导致大批在线用户被强制登出。

为了在读性能和安全性之间取得平衡，当会话缓存命中后，系统仍需单表查询 MySQL 确认用户的禁用状态。这确保了管理员在将某个恶意账户禁用后，该用户的会话会当即失效并被拦截器阻断，而不会因 Redis 缓存的存在而带来安全滞后。

---

## 面试与设计陈述重点

在面试或技术汇报中，可以这样表述该模块的设计精髓：
本系统采用服务端会话（Session Cookie）而非客户端 JWT 来管理登录状态，以确保对会话的绝对撤销控制。密码使用 Argon2id 安全存储，登录生成的 32 字节高熵令牌在 MySQL 中仅保存其 BLAKE2b 哈希，规避了数据库泄露带来的直接重放风险。Redis 仅作为会话加速缓存，缓存失效或宕机时能无缝回退 MySQL 容灾。为防止安全延迟，缓存命中后仍会级联校验用户禁用状态，而过期行则通过 MySQL 内部的事件调度器每天自动定时清理。

---

## 代码阅读路线

若想顺着代码理清整条调用链，建议按照以下顺序阅读：

* **路由与响应头设置**：[AuthApiRouter.cpp](file:///home/wxm/FileLink/src/auth/AuthApiRouter.cpp)，关注登录、登出和 Cookie 属性的设置逻辑。
* **Cookie 拦截与提取**：[RequestAuthenticator.cpp](file:///home/wxm/FileLink/src/auth/RequestAuthenticator.cpp)，了解如何解析 HTTP 请求头提取 Token。
* **业务流控与回退**：[AuthService.cpp](file:///home/wxm/FileLink/src/auth/AuthService.cpp)，阅读 `current_user` 校验逻辑，观察 Redis 与 MySQL 的主备配合。
* **密码哈希封装**：[PasswordHasher.cpp](file:///home/wxm/FileLink/src/auth/PasswordHasher.cpp)，了解 libsodium 接口的封装。
