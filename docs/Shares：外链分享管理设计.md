# Shares：外链分享管理设计

在 FileLink 系统中，[src/shares/](file:///home/wxm/FileLink/src/shares/) 模块负责管理公开外链分享体系。它使用随机高熵安全令牌（Token），并通过单向哈希防泄露设计、限时提取机制以及只读权限映射，为私有逻辑文件提供安全的公开下载通道。

本篇文档将详细剖析 `shares` 模块的路由注册、API 处理、服务层业务逻辑及具体的 C++ 代码实现。

---

## 路由注册与委托分发

由于 Tudou HTTP 框架的前缀匹配限制，分享管理接口（如 `/files/<file_id>/shares`）与文件私有接口共享 `/files/` 前缀。为了不破坏模块化隔离，系统采用路由委托分发设计。

### 路由分发与委托

在 [ShareApiRouter::handle_management_request](file:///home/wxm/FileLink/src/shares/ShareApiRouter.cpp#L45) 中拦截并分发管理请求：

```cpp
bool ShareApiRouter::handle_management_request(const HttpRequest& request, HttpResponse& response) {
    const std::string path = request.get_path();
    std::string file_id;
    
    // 1. 判断是否是创建或列出分享请求：POST/GET /files/<file_id>/shares
    if (parse_shares_path(path, file_id)) {
        if (request.get_method() == "POST") {
            handle_create(request, response, file_id);
        } else if (request.get_method() == "GET") {
            handle_list(request, response, file_id);
        } else {
            response = json_response(405, "Method Not Allowed", {{"message", "Method Not Allowed"}});
        }
        return true;
    }

    // 2. 判断是否是撤销分享请求：DELETE /files/<file_id>/shares/<share_id>
    std::string share_id;
    if (parse_shares_revoke_path(path, file_id, share_id)) {
        if (request.get_method() == "DELETE") {
            handle_revoke(request, response, file_id, share_id);
        } else {
            response = json_response(405, "Method Not Allowed", {{"message", "Method Not Allowed"}});
        }
        return true;
    }
    
    return false; // 非分享管理请求，交给文件路由处理
}
```

### 匿名公开下载接口

对于外界匿名的公开文件提取，[ShareApiRouter::register_public_routes](file:///home/wxm/FileLink/src/shares/ShareApiRouter.cpp#L38) 独立注册了 `/shares/` 前缀路由，在不需要登录 Cookie 会话的情况下完成安全下载：

```cpp
void ShareApiRouter::register_public_routes() {
    // 匿名下载路由格式：GET /shares/<token>/download
    server_.add_prefix_route("/shares/", [this](const HttpRequest& request, HttpResponse& response) {
        if (request.get_method() == "GET") {
            handle_public_download(request, response);
        } else {
            response = json_response(405, "Method Not Allowed", {{"message", "Method Not Allowed"}});
        }
    });
}
```

---

## 业务服务与安全哈希控制

[ShareService](file:///home/wxm/FileLink/src/shares/ShareService.h#L48) 承担了所有外链生成的防暴力破译和生命周期校验。为了防止数据库泄露导致外链被非法遍历重放，数据库只存令牌的 BLAKE2b 哈希值。

### 创建分享链接（单向哈希存储）

在 [create_share](file:///home/wxm/FileLink/src/shares/ShareService.cpp#L58) 中，生成高熵随机数并计算哈希落库：

```cpp
CreateShareResult ShareService::create_share(const std::string& owner_user_id,
    const std::string& file_id,
    std::time_t expires_at,
    CreatedShare& out_share) {
    // 验证生存期合法性 (严禁过期或无效时间)
    if (expires_at <= std::time(nullptr)) {
        return CreateShareResult::InvalidExpiry;
    }
    if (sodium_init() < 0) {
        return CreateShareResult::SystemError;
    }

    try {
        db::SociSessionLease lease(pool_);
        soci::session& sql = lease.get();
        db::File file;
        // 校验文件所有权，防越权分享
        if (!db::FileDao(sql).find_by_id_and_owner(file_id, owner_user_id, file)) {
            return CreateShareResult::FileNotFound;
        }

        // 1. 利用 libsodium 生成 32 字节高熵随机会话令牌
        const std::string token = random_bytes(crypto_generichash_BYTES);
        
        db::Share share;
        share.share_id = random_bytes(16);
        share.file_id = file_id;
        // 2. 对原始令牌进行 BLAKE2b 哈希，仅哈希值入库
        share.token_hash = hash_token(token);
        share.expires_at = local_time(expires_at);

        soci::transaction transaction(sql);
        db::ShareDao(sql).create(share);
        transaction.commit();

        // 3. 原始 token 只在此处返回给客户端一次，后续不再提供读取渠道
        out_share = { share.share_id, encode_token(token), share.expires_at };
        return CreateShareResult::Success;
    } catch (const std::exception&) {
        return CreateShareResult::SystemError;
    }
}
```

### 公开分享有效性匹配与提取

在 [find_shared_file](file:///home/wxm/FileLink/src/shares/ShareService.cpp#L109) 中，解析 URL 中的 Hex 文本，计算哈希后在数据库中进行活性与有效期限判断：

```cpp
bool ShareService::find_shared_file(const std::string& token, db::File& out_file) {
    if (sodium_init() < 0) {
        return false;
    }

    std::string raw_token;
    // 解密 Hex 编码还原二进制令牌
    if (!decode_token(token, raw_token)) {
        return false;
    }

    try {
        db::SociSessionLease lease(pool_);
        soci::session& sql = lease.get();
        db::Share share;
        
        // 计算 BLAKE2b(raw_token) 并查询有效分享
        // SQL 条件包含：revoked_at IS NULL AND expires_at > NOW()
        if (!db::ShareDao(sql).find_active_by_token_hash(hash_token(raw_token), share)) {
            return false;
        }
        
        // 提取关联逻辑文件的元数据
        return db::FileDao(sql).find_by_id(share.file_id, out_file);
    } catch (const std::exception&) {
        return false;
    }
}
```
