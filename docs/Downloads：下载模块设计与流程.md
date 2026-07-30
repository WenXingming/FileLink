# Downloads：下载模块设计与流程

本文对应当前 <code>src/downloads/</code> 实现，目标是帮助读者理解私有下载与分享下载如何完成授权、定位物理对象，并把大文件交给 Nginx 发送。

## 一句话概括

Downloads 模块只负责“允许下载哪个对象”：FileLink 验证文件所有权或分享 Token，返回 <code>X-Accel-Redirect</code>；真正的文件字节由 Nginx 从对象目录发送，不经过 FileLink 应用内存。

## 模块地图

下载 HTTP 流程采用 Parser、Controller、Service、View 分工：

~~~text
HttpRequest
    │
    ▼
DownloadRequestParser  识别私有下载路径或分享 Token
    │
    ▼
DownloadApiRouter      Controller：选择授权方式并编排请求
    │
    ▼
DownloadService        Model / Application Service：取得文件并定位对象
    │
    ▼
DownloadResponseView   View：构造下载 Header 和 X-Accel-Redirect
    │
    ▼
HttpResponse
    │
    ▼
Nginx                   读取对象文件并发送给客户端
~~~

两条下载路径在 Router 中分开，在 Service 中汇合：

~~~text
私有下载：Session + file_id ─► AuthService ─► FileService ─► db::File
分享下载：token ─────────────────────────────► ShareService ─► db::File

db::File
    ▼
ObjectStore::get_object_key
    ▼
DownloadTarget
    ▼
X-Accel-Redirect ─► Nginx
~~~


| 文件                                                              | 单一职责                                              |
| ------------------------------------------------------------------- | ------------------------------------------------------- |
| [DownloadRequestParser](../src/downloads/DownloadRequestParser.h) | 解析私有 file_id 或公开分享 Token                     |
| [DownloadApiRouter](../src/downloads/DownloadApiRouter.h)         | 注册下载路由，选择私有或分享流程并映射错误            |
| [DownloadService](../src/downloads/DownloadService.h)             | 复用 FileService、ShareService，并生成 DownloadTarget |
| <code>DownloadTarget</code>                                       | 保存对象 Key 和客户端可见文件名的简单输出 DTO         |
| [DownloadResponseView](../src/downloads/DownloadResponseView.h)   | 构造下载 Header、内部重定向和错误响应                 |

模块没有下载接口层、策略类、工厂或 DI 容器。私有和分享下载只有授权入口不同，不需要两个 Service 实现。

## 为什么独立成 Downloads

重构前，私有下载位于 Files，分享下载位于 Shares。两个模块都需要：

- 把 content_hash 编码为对象 Key；
- 使用 ObjectStore 的静态对象 Key 规则；
- 构造 Content-Disposition；
- 构造 X-Accel-Redirect；
- 处理相似的下载错误。

这些逻辑分散后，FileApiRouter 和 ShareApiRouter 都承担了“文件交付”职责。现在边界变为：

~~~text
Files       管理用户拥有的逻辑文件
Shares      管理公开分享授权
Downloads   管理下载交付
ObjectStore 管理内容哈希到对象路径的映射
Nginx       发送真实文件字节
~~~

Downloads 是一个真实业务边界，而不是单纯为了复用代码建立的工具目录。

## HTTP 接口


| 方法与路径                                    | 输入                    | 成功响应                         | 授权依据           |
| ----------------------------------------------- | ------------------------- | ---------------------------------- | -------------------- |
| <code>GET /downloads/private/{file_id}</code> | Session Cookie、file_id | <code>200 OK</code> + 内部重定向 | 文件属于当前用户   |
| <code>GET /downloads/shared/{token}</code>    | 分享 Token              | <code>200 OK</code> + 内部重定向 | 分享未撤销且未过期 |

两条接口都只接受 GET。其他方法或无法识别的下载路径返回 <code>404</code>。

输入格式：


| 参数    | 原始长度 | URL 表示          |
| --------- | ---------- | ------------------- |
| file_id | 16 字节  | 32 位小写十六进制 |
| token   | 32 字节  | 64 位小写十六进制 |

## 请求解析层：DownloadRequestParser

[DownloadRequestParser.cpp](../src/downloads/DownloadRequestParser.cpp) 只识别两种固定路径：

~~~text
/downloads/private/{32 位小写十六进制 file_id}
/downloads/shared/{64 位小写十六进制 token}
~~~

### 私有文件 ID

<code>parse_private_file_id</code>：

1. 确认路径以 <code>/downloads/private/</code> 开头。
2. 确认剩余部分没有斜杠。
3. 确认 ID 恰好为 32 位小写十六进制。
4. 解码为 16 字节二进制 file_id。

### 分享 Token

<code>parse_shared_token</code>：

1. 确认路径以 <code>/downloads/shared/</code> 开头。
2. 确认剩余部分没有斜杠。
3. 确认 Token 恰好为 64 位小写十六进制。
4. 保留十六进制文本，交给 ShareService 解码和哈希。

Parser 不读取 Cookie、不查询数据库，也不判断 Token 是否有效。

## Controller：DownloadApiRouter

[DownloadApiRouter.cpp](../src/downloads/DownloadApiRouter.cpp) 只持有：

~~~text
HttpServer
DownloadService
AuthService
~~~

<code>register_routes</code> 直接注册私有下载、分享下载和未知下载路径三个入口：

~~~text
不是 GET
    └──► 404

匹配 /downloads/private/{file_id}
    └──► handle_private_download

匹配 /downloads/shared/{token}
    └──► handle_shared_download

均不匹配
    └──► 404
~~~

Router 知道私有下载需要 Session、分享下载不需要 Session，但不知道文件如何查询、Token 如何哈希或对象目录如何组织。

## 私有下载流程

~~~text
GET /downloads/private/{file_id} + Session Cookie
    ▼
DownloadRequestParser：解析 file_id
    ├── 失败 ─► 404 Not Found
    └── 成功
          ▼
AuthRequestParser：读取 Session Cookie
    ├── 缺少 ─► 401 Unauthorized
    └── 成功
          ▼
AuthService::current_user
    ├── 无效 Session ─► 401
    ├── 系统错误      ─► 500
    └── 成功
          ▼
DownloadService::find_private_download(user_id, file_id)
          ▼
FileService::find_file(user_id, file_id)
    ├── 不存在或不属于用户 ─► 404
    └── 成功
          ▼
生成 DownloadTarget
          ▼
DownloadResponseView::file
~~~

<code>FileService::find_file</code> 使用 <code>file_id + owner_user_id</code> 联合查询，因此所有权检查和文件查询是同一个数据库操作。

无权访问与文件不存在都返回 <code>404</code>，不会泄露其他用户文件是否存在。

## 分享下载流程

~~~text
GET /downloads/shared/{token}
    ▼
DownloadRequestParser：校验 64 位 Token
    ├── 失败 ─► 404 Not Found
    └── 成功
          ▼
DownloadService::find_shared_download(token)
          ▼
ShareService::find_shared_file(token)
    ▼
解码 Token 并计算 token_hash
    ▼
查询 revoked_at IS NULL 且 expires_at > NOW() 的 Share
    ├── 无效、过期或已撤销 ─► 404
    └── 成功
          ▼
查询 Share 指向的 db::File
          ▼
生成 DownloadTarget
          ▼
DownloadResponseView::file
~~~

分享下载不会读取 Session Cookie。访问能力完全来自 URL 中的高熵 Token。

## Service：DownloadService

[DownloadService.cpp](../src/downloads/DownloadService.cpp) 负责把“获得访问许可的文件”转换为“可以交付的下载目标”。

输出 DTO 很小：

~~~cpp
struct DownloadTarget {
    std::string object_key;
    std::string display_name;
};
~~~

其中：

- <code>object_key</code> 是 Nginx 内部位置使用的相对对象路径。
- <code>display_name</code> 是浏览器保存文件时看到的名称。

两条 Service 方法的差异只在取得 <code>db::File</code> 的方式：

~~~text
find_private_download：FileService + owner_user_id + file_id
find_shared_download： ShareService + token
~~~

取得文件后统一执行：

~~~text
db::File.content_hash
    ▼ 十六进制编码
64 位内容哈希
    ▼ ObjectStore::get_object_key
aa/bb/<完整内容哈希>
    ▼
DownloadTarget
~~~

DownloadService 不打开对象文件，也不构造 HttpResponse。

## View：DownloadResponseView

[DownloadResponseView.cpp](../src/downloads/DownloadResponseView.cpp) 把 DownloadTarget 转换为：

~~~http
HTTP/1.1 200 OK
Content-Type: application/octet-stream
Content-Disposition: attachment; filename="report.pdf"
X-Accel-Redirect: /_filelink_objects/aa/bb/<content-hash>
~~~

响应 Body 为空。

View 还会清理下载文件名中的控制字符、双引号、反斜杠和非 ASCII 字节；清理后为空时使用 <code>download</code>。

错误响应统一为 JSON：

~~~json
{"message":"Not Found"}
~~~

View 不访问数据库、不检查权限，也不读取文件。

## Nginx X-Accel-Redirect 机制

[config/nginx.conf](../config/nginx.conf) 配置了内部对象位置：

~~~nginx
location /_filelink_objects/ {
    internal;
    alias /srv/filelink/storage/objects/;
}
~~~

完整交付过程：

~~~text
客户端请求公开下载 URL
    ▼
Nginx 把请求代理给 FileLink
    ▼
FileLink 完成授权，只返回 X-Accel-Redirect
    ▼
Nginx 在内部把 URL 映射到 storage/objects
    ▼
Nginx 发送文件字节
~~~

<code>internal</code> 表示客户端不能直接请求 <code>/_filelink_objects/...</code>。只有上游应用返回的内部重定向可以进入该位置。

这保证了“FileLink 决定权限，Nginx 负责传输”。**收益：大文件不进入 C++ 应用的响应 Body，FileLink 的响应内存不会随文件大小线性增长，Tudou 事件循环也不需要执行文件读取循环**。

## 大文件的内存模型

“使用 Nginx 发送超大文件一定不会 OOM”过于绝对。更准确的结论是：

> 单个下载请求的应用层内存不会随文件大小线性增长；文件大小本身不会让 FileLink 把整个文件加载到内存。

需要分开看三层内存。

### 1. FileLink 应用进程

FileLink 的下载代码只处理：

- file_id、Token 和 Session；
- 一条 File 数据库记录；
- content_hash、object_key 和 display_name；
- 一个很小的 HttpResponse。

它不会：

- 把对象文件读入 <code>std::string</code>；
- 在应用层建立读取文件的循环；
- 把整个文件写入 HttpResponse Body；
- 通过 FileLink 的用户态缓冲区转发文件字节。

因此 FileLink 进程的内存占用不会随文件大小线性增长。

### 2. Nginx 用户态内存

Nginx 仍然需要请求状态、Header 和连接缓冲区等内存。这些开销与并发连接数和配置有关，不应固定表述为“每个连接只有十几 KB”。

开启 <code>sendfile</code> 时，Nginx 可以让操作系统在文件和 socket 之间传递数据，避免先把文件块拷贝到 Nginx 的用户态缓冲区。这是常说的“零拷贝”，但它表示减少用户态数据拷贝，不表示整个系统不使用内存。

### 3. Linux 内核内存

操作系统仍会使用 Page Cache、socket 发送缓冲区和 TCP 状态。数据由 TCP 背压和事件循环逐步发送，不会因为文件是 100 GB 就一次性读入 100 GB 物理内存；内核在内存紧张时可以回收文件缓存页。

但是，极高并发、过大的缓冲配置、其他进程的内存竞争或过低的容器限额仍然可能导致内存压力。因此应说“单个请求的内存开销受缓冲策略约束，不随文件大小线性增长”，而不是“绝对不会爆内存”。

## 100 GB 文件的 Content-Length 与流式发送

[DownloadResponseView.cpp](../src/downloads/DownloadResponseView.cpp) 不设置文件的 <code>Content-Length</code>，也不把文件放入响应 Body。FileLink 只返回空 Body、下载文件名和 <code>X-Accel-Redirect</code>。

Nginx 收到内部重定向后才打开目标文件、读取文件元数据，并向客户端生成最终响应。假设文件大小是 100 GiB，客户端会先收到：

~~~http
HTTP/1.1 200 OK
Content-Length: 107374182400
Content-Type: application/octet-stream
Content-Disposition: attachment; filename="large.bin"

<文件内容持续传输>
~~~

<code>Content-Length</code> 只表示完整响应体的总字节数，不表示这些字节已经在内存中。实际传输过程是：

~~~text
Nginx 发送当前可发的一段文件
    ▼
socket 写满或客户端跟不上
    ▼
Nginx 回到事件循环，等待 socket 再次可写
    ▼
继续发送下一段，直到完成或连接中断
~~~

开启 <code>sendfile</code> 时，Nginx 反复执行 <code>sendfile()</code>，每次最多尝试发送 <code>sendfile_max_chunk</code> 限制的数据。实际发送量还可能因 TCP 背压而更小。

~~~text
Content-Length = 文件总大小
单次发送量  = 受 sendfile_max_chunk 和 socket 状态约束
常驻内存量  ≠ 文件总大小
~~~

如果客户端只请求一段字节范围，Nginx 会返回 <code>206 Partial Content</code>；此时 <code>Content-Length</code> 是当前范围的长度，<code>Content-Range</code> 同时说明它在完整文件中的位置。

## 传输中断与断点续传

### 当前传输中断时

如果客户端关闭下载、网络断开或长时间无法继续接收，Nginx 会在 socket 事件或后续写操作中发现连接异常，然后结束当前请求。请求结束后，Nginx 关闭该请求打开的文件和 socket，并回收请求状态。下载中断不会修改内容寻址对象文件。

响应头如果已经发出，这次请求的日志状态通常仍然是 <code>200</code>，因为服务器无法在响应中途改写已发送的状态行。Nginx 默认 <code>combined</code> access log 中的 <code>$body_bytes_sent</code> 会记录 Nginx 向 socket 写出的响应体字节数，而不是声明的完整 <code>Content-Length</code>。

如果需要在日志中明确区分完成与中断，可以在自定义 <code>log_format</code> 中加入 <code>$request_completion</code>；完整请求的值为 <code>OK</code>，未完成时为空。

### 断点续传是一次新请求

Nginx 不会在服务端保存“上次已经传了 10 GiB”这样的下载会话。客户端必须保留已下载的局部文件，然后使用新的 Range 请求表明希望继续的偏移量：

~~~http
GET /downloads/private/<file_id> HTTP/1.1
Cookie: filelink_session=...
Range: bytes=10737418240-
~~~

这个新请求仍然经过完整的 FileLink 流程：

~~~text
重新解析下载 URL
    ▼
重新验证 Session 和文件所有权，或重新验证分享 Token
    ▼
FileLink 再次返回 X-Accel-Redirect
    ▼
Nginx 处理 Range，从指定偏移量发送剩余字节
~~~

对于 100 GiB 文件，从 10 GiB 偏移量继续时，正确的字节范围是：

~~~http
HTTP/1.1 206 Partial Content
Content-Range: bytes 10737418240-107374182399/107374182400
Content-Length: 96636764160
~~~

字节下标从 0 开始，所以最后一个字节下标是总长度减 1。上述响应体是剩余的 90 GiB。

续传能否成功还取决于新请求的授权状态：

- 私有下载的 Session 必须仍然有效，文件也必须仍属于当前用户。
- 分享下载的 Token 不能已过期或被撤销。
- 文件被删除后，新请求无法通过 FileLink 的文件查询，即使物理对象尚未被 Cleaner 回收也不能续传。

当前端到端测试已覆盖单区间 Range 请求和 <code>206 Content-Range</code>。对于达到 <code>directio 1g</code> 阈值的文件，Nginx 使用 Direct I/O 路径从该偏移量继续读取，而不是调用 <code>sendfile()</code>；HTTP Range 语义不受这个底层 I/O 差异影响。

### Range 在 X-Accel-Redirect 链路中的流向

<code>Range</code> 是客户端发给服务器的请求头，不是 FileLink 应该返回的响应头。Nginx 是对外 HTTP 连接的真正入口，它在收到客户端请求时已经保存了原始 <code>Range</code> 请求上下文。

完整链路是：

~~~text
客户端
    │ GET /downloads/private/<file_id>
    │ Range: bytes=10737418240-
    ▼
Nginx 保存原始请求与 Range
    │ proxy_pass；原始请求头默认也会转发给 FileLink
    ▼
FileLink 验证权限并查找对象
    │ 不解析 Range，不读取文件
    │ 返回 X-Accel-Redirect: /_filelink_objects/<object_key>
    ▼
Nginx 在同一请求中执行内部重定向
    │ 使用原始 Range 处理静态对象文件
    ▼
客户端
    │ 206 Partial Content
    │ Content-Range + 当前区间的 Content-Length
~~~

[DownloadResponseView.cpp](../src/downloads/DownloadResponseView.cpp) 因此只需构造：

~~~http
Content-Type: application/octet-stream
Content-Disposition: attachment; filename="large.bin"
X-Accel-Redirect: /_filelink_objects/<object_key>
~~~

View 不应设置 <code>Range</code>：那会把请求头错误地复制为响应头。View 也不应自行计算 <code>Content-Range</code>，因为 FileLink 没有打开对象文件，也不负责规范化 Range 或发送字节。最终的 <code>206</code>、<code>Content-Range</code> 和 <code>Content-Length</code> 都由 Nginx 生成。

当前 [config/nginx.conf](../config/nginx.conf) 没有禁用 Range，而端到端测试也已确认这条链路能返回 <code>206</code> 和正确的 <code>Content-Range</code>。Nginx 对原始请求头的默认转发与 <code>X-Accel-Redirect</code> 内部重定向语义参见 [Nginx Proxy Module](https://nginx.org/en/docs/http/ngx_http_proxy_module.html#proxy_pass_request_headers)。

### 是否需要额外 Nginx 配置

当前不需要为支持断点续传增加 <code>max_ranges 100</code>。Nginx 默认已支持 Range；<code>max_ranges</code> 的作用是限制一个请求允许的字节区间数量，值为 0 才会禁用 Range。FileLink 的续传只需要单区间请求；如果未来需要限制多区间请求，可以明确设置 <code>max_ranges 1</code>。

<code>reset_timedout_connection on</code> 也不是断点续传必须配置。Nginx 原本就会结束已断开的请求；该指令只是让“已超时的连接”在关闭时通过 <code>SO_LINGER</code> 发送 TCP RST，避免长时间留在 <code>FIN_WAIT1</code>。是否开启应由真实连接压力和运维需求决定。Nginx 默认的 <code>send_timeout 60s</code> 会在两次向客户端写入操作之间超时时关闭连接，它不是整个大文件下载的总时限。

官方语义参见 [Nginx Core Module](https://nginx.org/en/docs/http/ngx_http_core_module.html#max_ranges) 和 [Nginx Log Module](https://nginx.org/en/docs/http/ngx_http_log_module.html)。

## sendfile 是部署能力，不是 FileLink 业务代码

<code>X-Accel-Redirect</code> 只负责让 Nginx 执行内部重定向，它不等于自动开启 <code>sendfile</code>。Nginx 是否使用 <code>sendfile</code> 取决于 Nginx 配置、运行平台和当前请求。

项目的 [config/nginx.conf](../config/nginx.conf) 是被官方 Nginx 镜像加载的 <code>server</code> 片段。内部下载 location 已经显式配置 <code>sendfile on</code>，因此不依赖镜像主配置的隐式继承；更换镜像或修改配置后，仍应用 <code>nginx -T</code> 检查最终生效值。

当前架构始终能够确定的是：

> 文件字节由 Nginx 读取和发送，不经过 FileLink 进程。

如果 <code>sendfile</code> 关闭，Nginx 仍然负责发送文件，只是用户态拷贝和资源开销会增加。因此“FileLink 使用 sendfile”也不准确，C++ 代码本身没有调用 <code>sendfile</code>。

## 三个常见 Nginx 参数如何理解

### sendfile_max_chunk

<code>sendfile_max_chunk</code> 限制单次 <code>sendfile()</code> 系统调用的传输量，避免一个高速连接长时间占用 worker。当前 Nginx 默认值已是 <code>2m</code>；是否改为 <code>1m</code> 应根据延迟与吞吐量测试决定，不是大文件下载的必选项。

### tcp_nopush

<code>tcp_nopush on</code> 只在使用 <code>sendfile</code> 时生效。它会尽量把响应 Header 与文件开头放在同一个包中，并以完整包发送文件。它是网络包组织优化，不是防止 OOM 的配置。

### directio

<code>directio size</code> 会让达到阈值的请求使用 Direct I/O，但 Nginx 也会自动禁用该请求的 <code>sendfile</code>。它可能减少 Page Cache 污染，同时也引入对齐、Range 请求和 I/O 路径方面的取舍。不应只因为“文件大”就盲目开启，而应根据实际存储介质、访问热度和压测结果决定。

### FileLink 当前建议

为了不依赖 Docker 镜像主配置的隐式继承，可以在内部下载 location 中显式配置：

~~~nginx
location /_filelink_objects/ {
    internal;
    alias /srv/filelink/storage/objects/;

    sendfile on;
    sendfile_max_chunk 2m; # 限制单次 sendfile() 传输量
    tcp_nopush on;         # 尽量合并响应头并发送完整数据包

    aio threads;
    directio 1g;           # 达到阈值的文件改用 Direct I/O
}
~~~

<code>1m</code> 也是合法的 <code>sendfile_max_chunk</code> 值，会更频繁地让 worker 回到事件循环，但也会增加系统调用次数。在没有压测数据前，保留现代 Nginx 默认的 <code>2m</code> 更简单。

当前配置以 <code>1g</code> 作为保守的 Direct I/O 起点：小于 1 GiB 的文件继续使用 <code>sendfile</code>，达到阈值的文件自动禁用 <code>sendfile</code>，改用 <code>directio</code>。<code>aio threads</code> 把这条路径的文件读取放到 Nginx 线程池，避免 worker 直接等待磁盘 I/O。

这个 <code>1g</code> 阈值不是 Nginx 通用最优值。它应在有真实大文件负载后，根据 Page Cache 命中率、worker 延迟、磁盘吞吐量和 Range 下载表现再调整或删除。

官方指令语义参见 [Nginx Core Module](https://nginx.org/en/docs/http/ngx_http_core_module.html#sendfile) 和 [Nginx Proxy Module](https://nginx.org/en/docs/http/ngx_http_proxy_module.html#proxy_ignore_headers)。

## 与上传链路的区别

~~~text
上传
客户端分块
    ▼
Tudou / FileLink 接收当前分块
    ▼
逐块写入临时文件
    ▼
完成后提交为内容寻址 Object

下载
客户端请求下载
    ▼
FileLink 验证授权并定位 Object
    ▼
X-Accel-Redirect
    ▼
Nginx 发送 Object
~~~

- 上传通过分块和落盘，避免整个请求体常驻内存。
- 下载通过应用授权与静态文件交付分离，避免文件内容进入应用内存。

## 模块边界与依赖方向

~~~text
DownloadApiRouter ─► AuthService
                  ├► DownloadService ─► FileService
                  │                  ├► ShareService
                  │                  └► ObjectStore::get_object_key
                  └► DownloadResponseView
~~~

关键方向是：

- Downloads 依赖 Files 提供的所有权查询。
- Downloads 依赖 Shares 提供的 Token 授权查询。
- Files 和 Shares 不反向依赖 Downloads。
- ObjectStore 不知道用户、分享或 HTTP。
- Nginx 不参与业务授权，只执行内部文件交付。

## 组合根

[main.cpp](../src/main.cpp) 显式连接下载模块：

~~~cpp
filelink::DownloadService downloadService(fileService, shareService);
filelink::DownloadApiRouter downloadRouter(server, downloadService, authService);
downloadRouter.register_routes();
~~~

DownloadService 只注入授权流程需要的 FileService 和 ShareService；对象 Key 是无状态存储规则，不需要注入 ObjectStore 实例。

## 测试地图


| 测试                                                        | 覆盖内容                                             |
| ------------------------------------------------------------- | ------------------------------------------------------ |
| [DownloadHttpTests.cpp](../tests/DownloadHttpTests.cpp)     | 私有路径、分享 Token、非法输入和内部重定向响应       |
| [DownloadServiceTest.cpp](../tests/DownloadServiceTest.cpp) | 私有所有权、有效 Token 和撤销后失效                  |
| [DownloadTest.sh](../tests/DownloadTest.sh)                 | 真实服务、认证、Nginx、Range、分享与删除后的下载行为 |

端到端测试还验证：

- 私有下载没有 Cookie 时返回 <code>401</code>。
- 客户端直接访问内部对象 URL 时返回 <code>404</code>。
- Nginx 能处理字节范围请求。
- 分享下载不需要认证。

## 推荐代码阅读顺序

1. [DownloadApiRouter.cpp](../src/downloads/DownloadApiRouter.cpp)：先看私有与分享下载的完整顶层分支。
2. [DownloadRequestParser.cpp](../src/downloads/DownloadRequestParser.cpp)：理解两种 URL 输入。
3. [DownloadResponseView.cpp](../src/downloads/DownloadResponseView.cpp)：理解 FileLink 与 Nginx 的交接点。
4. [DownloadService.cpp](../src/downloads/DownloadService.cpp)：理解授权后的 File 如何变成对象 Key。
5. [FileService.cpp](../src/files/FileService.cpp)：理解私有下载所有权。
6. [ShareService.cpp](../src/shares/ShareService.cpp)：理解分享 Token 有效性。
7. [ObjectStore.cpp](../src/storage/ObjectStore.cpp)：理解内容哈希到对象路径的映射。
8. [nginx.conf](../config/nginx.conf)：理解内部文件交付。

阅读时始终沿着这条主线：

~~~text
HTTP 输入 → Parser → Router → 授权 Service → DownloadTarget → View
          → X-Accel-Redirect → Nginx → 客户端
~~~
