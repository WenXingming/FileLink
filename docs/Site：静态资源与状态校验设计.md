# Site：静态资源与状态校验设计

在 FileLink 系统中，[src/site/](file:///home/wxm/FileLink/src/site/) 模块负责主站的静态资源（单页应用的前端 HTML/JS/CSS）分发与基础的系统健康检查状态校验。它作为 Web 应用的用户端入口，确保浏览器能够低延迟、安全地加载静态资产，并为运维监控提供实时的活性探针。

本篇文档将详细剖析 `site` 模块的路由设计、防目录穿越安全控制、静态文件类型匹配机制及具体的 C++ 代码实现。

---

## 门户路由与适配器层

[SiteRouter](file:///home/wxm/FileLink/src/site/SiteRouter.h#L11) 主要负责监听站点的首屏及静态资产路由，并将读取文件的请求委托给静态资源读取服务。

### 路由监听与分发

在 [SiteRouter::register_routes](file:///home/wxm/FileLink/src/site/SiteRouter.cpp#L10) 中，使用 Tudou HTTP Server 的精确路由与前缀路由进行注册：

```cpp
void SiteRouter::register_routes() {
    // 1. 首页加载路由映射
    server_.add_get_route("/", [this](const HttpRequest& request, HttpResponse& response) {
        handle_index(request, response);
    });
    server_.add_get_route("/index.html", [this](const HttpRequest& request, HttpResponse& response) {
        handle_index(request, response);
    });

    // 2. 活性探针/健康检查接口
    server_.add_get_route("/health", [this](const HttpRequest& request, HttpResponse& response) {
        handle_health(request, response);
    });

    // 3. 静态资源前缀监听路由（CSS, JS, 图片等）
    server_.add_prefix_route("/static/", [this](const HttpRequest& request, HttpResponse& response) {
        handle_static(request, response);
    });
}
```

### 静态资源读取与响应装配

在 [handle_static](file:///home/wxm/FileLink/src/site/SiteRouter.cpp#L38) 中，处理函数接收前缀请求并根据文件扩展名解析出标准的 HTTP `Content-Type` 头：

```cpp
void SiteRouter::handle_static(const HttpRequest& request, HttpResponse& response) {
    try {
        const std::string path = request.get_path();
        std::string extension;
        const std::size_t dot_position = path.find_last_of('.');
        if (dot_position != std::string::npos) {
            extension = path.substr(dot_position);
            // 转换为小写，保证后缀匹配的鲁棒性
            for (char& character : extension) {
                character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            }
        }
        
        // 委托 StaticFileService 安全读取文件字节，并通过 ApiResponseView 封包返回
        response = ApiResponseView::file(static_file_service_.get_asset_content(path), extension);
    }
    catch (const std::invalid_argument& error) {
        response = ApiResponseView::error(403, error.what()); // 防穿越安全越界拦截
    }
    catch (const std::exception& error) {
        response = ApiResponseView::error(404, error.what()); // 物理不存在
    }
}
```

---

## 静态资源服务与防穿越逻辑

[StaticFileService](file:///home/wxm/FileLink/src/site/StaticFileService.h#L10) 专职负责物理文件的校验与载入。为了防止恶意客户端通过构造路径穿越攻击（Directory Traversal）非法读取服务器上的敏感文件（如 `/etc/passwd`），服务层对所有的 URI 进行强制安全审查。

### 安全读取逻辑实现

在 [StaticFileService::get_asset_content](file:///home/wxm/FileLink/src/site/StaticFileService.cpp#L12) 中：

```cpp
std::string StaticFileService::get_asset_content(const std::string& uriPath) const {
    // 1. 安全壁垒：强制拦截含有路径回溯符 ".." 的请求，杜绝路径穿越攻击
    if (uriPath.find("..") != std::string::npos) {
        throw std::invalid_argument("Forbidden: Directory traversal detected");
    }

    std::string filePath = webRoot_ + uriPath;
    
    // 2. 状态检查：利用 POSIX stat() 判定文件必须存在且是常规文件（排除目录和软连接）
    struct stat info;
    if (::stat(filePath.c_str(), &info) != 0 || !S_ISREG(info.st_mode)) {
        throw std::runtime_error("Not Found");
    }

    // 3. 流式读取：使用 binary 模式安全读入文件全部内容并返回
    std::ifstream ifs(filePath, std::ios::binary);
    if (!ifs) {
        throw std::runtime_error("Failed to open file");
    }
    
    return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}
```

---

## MIME 媒体类型推断

在响应装配中，[ApiResponseView::infer_mime_type](file:///home/wxm/FileLink/src/ApiResponseView.cpp#L67) 负责将常见前端后缀映射为标准的规范媒体类型：

| 扩展名 (Extension) | MIME 媒体类型 | 作用描述 |
| :--- | :--- | :--- |
| `.html` / `.htm` | `text/html` | 主网页架构展现 |
| `.css` | `text/css` | 页面层叠样式渲染 |
| `.js` | `application/javascript` | 单页客户端逻辑交互代码 |
| `.png` | `image/png` | 页面图像、Logo 渲染 |
| `.jpg` / `.jpeg` | `image/jpeg` | 相机等高细节图层渲染 |
| `.svg` | `image/svg+xml` | 矢量矢量线条图标渲染 |
| 其他类型 | `application/octet-stream` | 未知默认二进制流分发 |
