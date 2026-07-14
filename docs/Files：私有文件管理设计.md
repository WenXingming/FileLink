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

### 文件流式下载实现

在 [handle_download](file:///home/wxm/FileLink/src/files/FileApiRouter.cpp#L118) 中，为了防止大文件读取导致应用进程 OOM 崩溃，系统采用了 POSIX 文件描述符结合 Tudou HTTP 框架的零拷贝传输：

```cpp
void FileApiRouter::handle_download(const HttpRequest& request, HttpResponse& response) {
    const std::string suffix = "/download";
    const std::string path = request.get_path();
    std::string file_id;
    if (request.get_method() != "GET" || !parse_file_id_path(path, suffix, file_id)) {
        response = json_response(404, "Not Found", {{"message", "Not Found"}});
        return;
    }

    AuthenticatedUser user;
    if (!authenticate_request(request_authenticator_, request, user, response)) return;

    try {
        db::File file;
        // 严格的所有权与文件检索
        if (!file_service_.find_file(user.user_id, file_id, file)) {
            response = json_response(404, "Not Found", {{"message", "Not Found"}});
            return;
        }

        const std::string object_path = file_service_.object_path(file);
        struct stat info;
        // 使用 O_CLOEXEC 标志打开文件描述符，防止进程派生泄露
        const int descriptor = ::open(object_path.c_str(), O_RDONLY | O_CLOEXEC);
        if (descriptor == -1 || ::fstat(descriptor, &info) != 0 || !S_ISREG(info.st_mode)) {
            if (descriptor != -1) { ::close(descriptor); }
            response = json_response(404, "Not Found", {{"message", "Not Found"}});
            return;
        }

        // 装配文件流下载响应头
        response.set_status(200, "OK");
        response.set_header("Content-Type", "application/octet-stream");
        response.set_header("Content-Length", std::to_string(info.st_size));
        response.set_header("Content-Disposition", "attachment; filename=\"" + download_name(file.display_name) + "\"");
        
        // 核心：使用 ScopedFd 包装文件描述符，委托 HTTP 框架流式发包，不占用进程内存
        response.set_file_body(std::make_shared<ScopedFd>(descriptor), static_cast<size_t>(info.st_size));
    } catch (const std::exception&) {
        response = json_response(500, "Internal Server Error", {{"message", "File download failed"}});
    }
}
```

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

### 物理路径转换逻辑

在 [object_path](file:///home/wxm/FileLink/src/files/FileService.cpp#L44) 中，服务将数据库保存的 16 字节二进制哈希数据解码转换，并委托给 `ObjectStore` 获取磁盘的存放路径：

```cpp
std::string FileService::object_path(const db::File& file) const {
    std::stringstream stream;
    stream << std::hex << std::setfill('0');
    // 将二进制哈希流转换为标准的 64 位十六进制小写文本
    for (unsigned char value : file.content_hash) {
        stream << std::setw(2) << static_cast<int>(value);
    }
    // 委托 ObjectStore 计算分层物理路径 (e.g. storage/objects/ab/cd/abcdef...)
    return store_.get_object_path(stream.str());
}
```
