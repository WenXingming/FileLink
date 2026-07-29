# Files：私有文件管理设计

在 FileLink 系统中，[src/files/](file:///home/wxm/FileLink/src/files/) 模块负责管理当前登录用户的逻辑文件资产。它提供了用户私有逻辑文件的列表查询、安全下载管道以及两阶段级联删除事务。

本篇文档将详细剖析 `files` 模块的路由注册、API 处理函数、服务层逻辑及具体的 C++ 代码实现。

---

## 路由注册与 HTTP 适配层

[FileApiRouter](file:///home/wxm/FileLink/src/files/FileApiRouter.h#L18) 负责对外注册 `/files` 相关的 HTTP 请求路径，并通过 Cookie 请求头提取用户标识进行拦截鉴权。

### 路由分发逻辑

在 [FileApiRouter::register_routes](file:///home/wxm/FileLink/src/files/FileApiRouter.cpp#L100) 中，路由使用 Tudou HTTP 框架的前缀匹配规则进行注册：

```cpp
void FileApiRouter::register_routes() {
    // 1. 获取当前用户的所有逻辑文件列表
    server_.add_get_route("/files", [this](const HttpRequest& request, HttpResponse& response) {
        handle_list_files(request, response);
    });

    // 2. 带参数的逻辑文件前缀匹配（下载、删除、外链分享管理）
    server_.add_prefix_route("/files/", [this](const HttpRequest& request, HttpResponse& response) {
        // 优先将管理请求委托给分享路由处理
        if (share_api_router_.handle_management_request(request, response)) {
            return;
        }
        
        if (request.get_method() == "GET") {
            handle_download(request, response);
        } else if (request.get_method() == "DELETE") {
            handle_delete(request, response);
        } else {
            response = json_response(404, "Not Found", {{"message", "Not Found"}});
        }
    });
}
```

### 下载授权与 Nginx 内部重定向

在 `handle_download` 中，FileLink 只负责认证、所有权检查和对象键转换，不读取文件正文：

```cpp
db::File file;
if (!file_service_.find_file(user.user_id, file_id, file)) {
    response = json_response(404, "Not Found", {{"message", "Not Found"}});
    return;
}

const std::string object_key = object_store_.get_object_key(hex_encode(file.content_hash));
response = ApiResponseView::download_redirect(object_key, file.display_name);
```

`download_redirect` 返回空响应体和 `X-Accel-Redirect`。Nginx 校验该内部地址、映射只读对象目录并发送文件，因此 FileLink 进程内存不随下载文件大小增长。完整数据路径、`sendfile` 前提和 Range 行为见[大文件上传与下载数据路径](大文件上传与下载数据路径.md)。

---

## 业务服务与事务一致性控制

[FileService](file:///home/wxm/FileLink/src/files/FileService.h#L17) 负责对接数据库连接池，处理所有权查询与解绑事务。

### 级联删除事务实现

在 [delete_file](file:///home/wxm/FileLink/src/files/FileService.cpp#L23) 中，文件删除并非仅仅移除一行数据库记录，而是要同时级联递减物理对象的引用计数。该逻辑全程在数据库本地事务中执行：

```cpp
bool FileService::delete_file(const std::string& owner_user_id, const std::string& file_id) {
    db::SociSessionLease lease(pool_);
    soci::session& sql = lease.get();
    soci::transaction transaction(sql); // 开启 SOCI 会话事务

    db::File file;
    db::FileDao file_dao(sql);
    
    // 1. 验证所有权，防止越权删除他人的文件记录
    if (!file_dao.find_by_id_and_owner(file_id, owner_user_id, file)) {
        return false;
    }
    
    // 2. 从逻辑文件表 `files` 中移除记录
    if (!file_dao.remove_by_id_and_owner(file_id, owner_user_id)) {
        return false;
    }
    
    // 3. 递减对应物理对象的引用计数 (若引用计数降为 0，则自动打上 PENDING_DELETE 的物理回收标签)
    if (!db::ObjectDao(sql).remove_reference(file.content_hash)) {
        throw std::runtime_error("logical file references a missing object"); // 异常触发事务回滚
    }

    transaction.commit(); // 提交事务完成原子物理释放
    return true;
}
```

### 服务职责边界

`FileService` 现在只负责逻辑文件的数据库查询和删除事务，不依赖 `ObjectStore`。物理对象键由需要下载能力的 Router 通过 `ObjectStore` 生成，文件正文则由 Nginx 发送。
