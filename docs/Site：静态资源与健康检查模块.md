# Site：静态资源与健康检查模块

本文对应当前 `src/site/` 实现，帮助读者从 HTTP 路由开始，理解首页、静态资源和健康检查响应是如何生成的。

## 一句话概括

Site 模块把 `web-root` 目录中的小型前端资源读取为字符串并返回给浏览器，同时提供一个不访问外部依赖的 `/health` 活性检查端点。

## 模块地图

静态资源请求采用轻量的 Controller、Service、View 分工：

```text
HttpRequest
    │
    ▼
SiteRouter          Controller：选择路由并编排一次请求
    │
    ▼
StaticFileService   Model / Application Service：校验路径并读取磁盘文件
    │
    ▼
SiteResponseView    View：构造状态码、Header 和 Body
    │
    ▼
HttpResponse
```


| 文件                                                   | 单一职责                                  |
| -------------------------------------------------------- | ------------------------------------------- |
| [`SiteRouter`](../src/site/SiteRouter.h)               | 注册站点路由，按请求结果选择响应          |
| [`StaticFileService`](../src/site/StaticFileService.h) | 校验资源路径并读取`web-root` 中的普通文件 |
| [`SiteResponseView`](../src/site/SiteResponseView.h)   | 构造健康检查、静态资源和站点错误响应      |

模块没有接口、工厂或 DI 容器。`main.cpp` 创建具体的 `StaticFileService`，再通过构造函数引用注入 `SiteRouter`。

## HTTP 接口


| 方法与路径           | 处理函数        | 成功响应 | 作用                       |
| ---------------------- | ----------------- | ---------- | ---------------------------- |
| `GET /`              | `handle_index`  | `200 OK` | 返回`web-root/index.html`  |
| `GET /index.html`    | `handle_index`  | `200 OK` | 返回同一个首页文件         |
| `GET /health`        | `handle_health` | `200 OK` | 返回进程活性状态           |
| `/static/*` 前缀路由 | `handle_static` | `200 OK` | 返回请求路径对应的静态资源 |

前三条是按 HTTP 方法和路径匹配的精确路由。Tudou 当前的前缀路由只匹配路径，因此 `/static/*` 本身不限制 HTTP 方法。

`/health` 的响应固定为：

```json
{"status":"ok"}
```

它只表明 HTTP 服务能够处理请求，不检查 MySQL、Redis、对象存储或磁盘空间，因此属于活性检查，而不是完整的依赖就绪检查。

## 组合与配置

[`main.cpp`](../src/main.cpp) 是组合根，负责连接 Site 模块：

```cpp
filelink::StaticFileService staticFileService(config.webRoot);
filelink::SiteRouter siteRouter(server, staticFileService);
siteRouter.register_routes();
```

静态资源根目录来自 `web-root`：

- 程序默认值是 `./web`。
- [`config/server.toml`](../config/server.toml) 中配置为 `web`。
- 命令行可以通过 `--web-root <目录>` 覆盖。

请求 URI 会直接拼接到该根目录。例如根目录为 `web` 时，`/static/app.js` 对应 `web/static/app.js`，`/index.html` 对应 `web/index.html`。

## Controller：SiteRouter

[`SiteRouter.cpp`](../src/site/SiteRouter.cpp) 只做 HTTP 编排。静态资源处理函数保持三步平铺流程：

```cpp
const std::string path = request.get_path();
const std::string content = static_file_service_.read_asset(path);
response = SiteResponseView::asset(path, content);
```

这三步分别是：

1. 从 Tudou 的 `HttpRequest` 取得 URI 路径。
2. 让 `StaticFileService` 读取资源内容。
3. 让 `SiteResponseView` 根据路径和内容构造响应。

Router 不解析文件扩展名、不维护 MIME 映射，也不执行 `stat` 或文件读取。

### 首页流程

```text
GET / 或 GET /index.html
    ▼
固定资源路径 /index.html
    ▼
StaticFileService::read_asset
    ├── 成功 ─► SiteResponseView::asset ─► 200
    └── 异常 ─► SiteResponseView::not_found("index.html not found") ─► 404
```

首页对外隐藏具体的磁盘错误，所有读取失败都返回固定的 `index.html not found`。

### 静态资源流程

```text
请求 /static/*
    ▼
读取 HttpRequest 中的实际路径
    ▼
StaticFileService::read_asset
    ├── invalid_argument ─► 403 Forbidden
    ├── 其他异常         ─► 404 Not Found
    └── 成功
          ▼
    SiteResponseView::asset ─► 200
```

异常类型是 Service 与 Router 之间的最小错误约定：路径不允许时抛出 `std::invalid_argument`，文件不存在或无法读取时抛出其他标准异常。

## Service：StaticFileService

[`StaticFileService.cpp`](../src/site/StaticFileService.cpp) 按以下顺序读取文件：

1. URI 中包含 `..` 时立即拒绝，避免路径回退到 `web-root` 之外。
2. 把 `webRoot_` 与 URI 路径拼接为物理路径。
3. 使用 `stat` 确认目标存在且最终指向普通文件。
4. 以二进制模式打开文件。
5. 把完整文件内容读入 `std::string` 并返回。

Service 不知道 HTTP 状态码。它只通过返回内容或抛出异常报告结果，具体映射由 Router 决定。

### 与大文件下载的区别

Site 模块会把静态资源完整读入进程内存，适合 HTML、CSS、JavaScript 和小图片等前端资源。它不是用户文件下载链路。

FileLink 的大文件下载由 File/Share 模块返回 `X-Accel-Redirect`，再由 Nginx 读取对象文件并发送；不会经过 `StaticFileService`。因此不要用 `/static/*` 提供用户上传的大文件。

## View：SiteResponseView

[`SiteResponseView.cpp`](../src/site/SiteResponseView.cpp) 集中维护 Site 模块对外可见的 HTTP 格式：

- `health_check`：返回 `200 OK` 和固定 JSON。
- `asset`：返回文件内容、`Content-Type` 和 `Content-Length`。
- `forbidden`：返回 `403 Forbidden` JSON。
- `not_found`：返回 `404 Not Found` JSON。

错误响应格式为：

```json
{"status":"error","message":"错误信息"}
```

View 根据请求路径的最后一个扩展名推断 MIME 类型，并在比较前转换为小写，因此 `.JS` 和 `.js` 得到相同结果。


| 扩展名          | Content-Type                            |
| ----------------- | ----------------------------------------- |
| `.html`         | `text/html; charset=utf-8`              |
| `.css`          | `text/css; charset=utf-8`               |
| `.js`           | `application/javascript; charset=utf-8` |
| `.txt`          | `text/plain; charset=utf-8`             |
| `.json`         | `application/json`                      |
| `.jpg`、`.jpeg` | `image/jpeg`                            |
| `.png`          | `image/png`                             |
| `.gif`          | `image/gif`                             |
| `.svg`          | `image/svg+xml`                         |
| `.pdf`          | `application/pdf`                       |
| `.mp4`          | `video/mp4`                             |
| 未知或无扩展名  | `application/octet-stream`              |

Router 只选择 `asset`、`forbidden` 或 `not_found`，不会重复拼装这些响应细节。

## 模块边界

Site 模块刻意不负责以下内容：

- 不进行用户认证或权限判断。
- 不查询 MySQL 或 Redis。
- 不访问内容寻址对象存储。
- 不处理上传和用户文件下载。
- 不检查系统所有依赖是否就绪。

因此阅读 Site 模块时，只需要理解 Tudou 路由回调、普通文件读取和 HTTP 响应构造三个概念。

## 测试与阅读顺序

[`SiteTests.cpp`](../tests/SiteTests.cpp) 覆盖健康检查响应、MIME 推断、错误 JSON、文件读取和目录回退拒绝。服务级 `/health` 检查位于 [`HealthCheckTest.sh`](../tests/HealthCheckTest.sh)。

运行单元测试：

```bash
ctest --test-dir build -R '^unit\.(SiteResponseViewTest|StaticFileServiceTest)' --output-on-failure
```

推荐按以下顺序阅读代码：

1. `SiteRouter::register_routes`：先知道模块有哪些入口。
2. `SiteRouter::handle_static`：理解一次完整请求如何被编排。
3. `StaticFileService::read_asset`：理解磁盘读取边界。
4. `SiteResponseView::asset`：理解 MIME 和响应构造。
5. `main.cpp`：理解对象如何创建和注入。
