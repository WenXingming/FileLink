# Files：私有文件模块设计与流程

本文对应当前 <code>src/files/</code> 实现，目标是帮助读者理解用户如何查看和删除自己拥有的逻辑文件，以及逻辑文件与物理对象之间的关系。

## 一句话概括

Files 模块管理“某个用户拥有哪些逻辑文件”：<code>FileApiRouter</code> 处理 HTTP 请求，<code>FileService</code> 查询或删除数据库记录，<code>FileResponseView</code> 构造响应；文件字节下载已经独立到 Downloads 模块。

## 模块地图

Files 采用 Parser、Controller、Service、View 的轻量分工：

~~~text
HttpRequest
    │
    ▼
FileRequestParser   解析 URL 中的十六进制 file_id
    │
    ▼
FileApiRouter       Controller：认证并编排一次 HTTP 请求
    │
    ▼
FileService         Model / Application Service：查询或删除逻辑文件
    │
    ▼
FileResponseView    View：构造状态码和 JSON
    │
    ▼
HttpResponse
~~~

这张图表示阅读顺序，而不是对象之间逐层转发。实际控制者始终是 Router：它按需调用 Parser、AuthService 和 FileService，最后选择一个 View。


| 文件                                                  | 单一职责                                                         |
| ------------------------------------------------------- | ------------------------------------------------------------------ |
| [FileRequestParser](../src/files/FileRequestParser.h) | 校验文件路径，并把 32 位十六进制 file_id 解码为 16 字节二进制 ID |
| [FileApiRouter](../src/files/FileApiRouter.h)         | 注册 Files 路由，编排请求解析、认证、Service 和 View             |
| [FileService](../src/files/FileService.h)             | 查询用户文件，并维护删除逻辑文件的数据库事务                     |
| [FileResponseView](../src/files/FileResponseView.h)   | 构造文件列表、删除成功和错误响应                                 |
| [FileDao](../src/database/File.h)                     | 执行 files 表的创建、查询和删除 SQL                              |

模块没有 Repository 接口、工厂或 DI 容器。<code>main.cpp</code> 创建具体对象，再通过构造函数引用进行连接。

## HTTP 接口


| 方法与路径                           | 输入                     | 成功响应                    | 作用                       |
| -------------------------------------- | -------------------------- | ----------------------------- | ---------------------------- |
| <code>GET /files</code>              | Session Cookie           | <code>200 OK</code>         | 列出当前用户拥有的逻辑文件 |
| <code>DELETE /files/{file_id}</code> | Session Cookie 和文件 ID | <code>204 No Content</code> | 删除当前用户拥有的逻辑文件 |

私有文件下载不属于 Files HTTP 接口，而是：

~~~text
GET /downloads/private/{file_id}
~~~

<code>file_id</code> 在 URL 中是 32 位小写十六进制文本，对应数据库中的 16 字节二进制 ID。大写字符、长度错误或多余路径都会解析失败。

## Controller：FileApiRouter

[FileApiRouter.cpp](../src/files/FileApiRouter.cpp) 只持有三个协作者：

~~~text
HttpServer
FileService
AuthService
~~~

它不持有 <code>ObjectStore</code>、<code>ShareApiRouter</code> 或 <code>DownloadService</code>，因此 Files Controller 不知道分享和下载如何实现。

Router 中的业务流程保持平铺：

~~~text
1. 解析需要的 URL 参数
2. 解析 Session Cookie 并认证当前用户
3. 调用 FileService
4. 选择 FileResponseView
~~~

认证复用 <code>AuthRequestParser::parse_session_token</code> 和 <code>AuthService::current_user</code>。Files 保留自己的错误 View，因此认证失败仍能按 Files 协议返回响应。

## 请求解析层：FileRequestParser

[FileRequestParser.cpp](../src/files/FileRequestParser.cpp) 只识别一种成员路径：

~~~text
/files/{32 位小写十六进制 file_id}
~~~

Parser 完成两件事：

1. 确认路径前缀、总长度和完整形状正确。
2. 将两位十六进制字符解码成一个字节，最终得到 16 字节 file_id。

Parser 不读取 Cookie、不查询数据库，也不决定失败应该返回 <code>401</code> 还是 <code>404</code>。

<code>GET /files</code> 没有路径参数，所以列表流程不需要调用 Parser。

## 列出文件流程

~~~text
GET /files + Session Cookie
    ▼
FileApiRouter：认证当前用户
    ├── 无效 Session ─► 401 Unauthorized
    └── 成功
          ▼
FileService::list_files(owner_user_id)
          ▼
FileDao::find_by_owner
          ▼
FileResponseView::file_list
          ▼
200 OK + JSON
~~~

DAO 查询带有 <code>owner_user_id</code> 条件，并按创建时间倒序排列，因此用户只能看到自己的逻辑文件。

响应只暴露客户端需要的字段：

~~~json
{
  "files": [
    {
      "file_id": "616c6963652d66696c652d6964303031",
      "name": "report.pdf"
    }
  ]
}
~~~

<code>owner_user_id</code>、<code>content_hash</code> 和数据库时间字段不会出现在响应中。

## 删除文件流程

~~~text
DELETE /files/{file_id}
    ▼
FileRequestParser：解析 file_id
    ├── 失败 ─► 404 Not Found
    └── 成功
          ▼
FileApiRouter：认证当前用户
    ├── 失败 ─► 401 / 500
    └── 成功
          ▼
FileService::delete_file(owner_user_id, file_id)
    ├── 文件不存在或不属于用户 ─► 404
    └── 删除成功                 ─► 204
~~~

<code>FileService::delete_file</code> 在一个数据库事务中完成：

~~~text
按 file_id + owner_user_id 查询逻辑文件
    ▼
删除 files 记录
    ▼
递减 objects.ref_count
    ▼
提交事务
~~~

所有权判断和文件查询是同一个 SQL 条件。文件不存在与文件属于其他用户都返回 <code>404</code>，不会泄露其他用户的文件是否存在。

如果逻辑文件引用的 Object 记录意外缺失，Service 抛出异常，事务自动回滚，Router 返回 <code>500</code>。

## 逻辑文件与物理对象

FileLink 中需要区分三种状态：


| 概念                   | 保存位置                                     | 含义                               |
| ------------------------ | ---------------------------------------------- | ------------------------------------ |
| 逻辑文件 File          | MySQL<code>files</code>                      | 用户可见的文件名、所有者和内容引用 |
| 物理对象 Object        | MySQL<code>objects</code> + 对象目录         | 按内容哈希去重后的不可变文件字节   |
| 上传会话 UploadSession | MySQL<code>upload_sessions</code> + 临时目录 | 尚未完成或正在收尾的分片上传       |

多个逻辑文件可以通过相同的 <code>content_hash</code> 引用同一个物理对象。因此删除 File 不能立即删除磁盘文件。

当最后一个逻辑文件被删除时：

~~~text
objects.ref_count：1 → 0
objects.state：READY → PENDING_DELETE
~~~

Cleaner 稍后负责安全回收物理对象。Files 只维护逻辑引用，不直接删除磁盘文件。

## View：FileResponseView

[FileResponseView.cpp](../src/files/FileResponseView.cpp) 集中维护 Files 的 HTTP 输出：

- <code>file_list</code>：把二进制 file_id 编码为小写十六进制并返回文件数组。
- <code>deleted</code>：返回 <code>204 No Content</code>。
- <code>unauthorized</code>：返回 <code>401</code> JSON。
- <code>not_found</code>：返回 <code>404</code> JSON。
- <code>server_error</code>：返回 <code>500</code> JSON。

View 不读取请求、不认证用户，也不访问数据库。

## 与相邻模块的边界

~~~text
Uploads ──完成上传──► 逻辑文件（files）
                           ▲
          ┌────────────────┼────────────────┐
          │                │                │
    FileService      ShareService     DownloadService
    列表与删除       分享授权         下载授权与交付
          │                                 │
          ▼                                 ▼
objects 引用计数 ─► Cleaner           ObjectStore
~~~

更准确的代码依赖方向是：

~~~text
DownloadService ─► FileService
DownloadService ─► ShareService
DownloadService ─► ObjectStore::get_object_key（静态对象布局规则）
~~~

Files 和 Shares 不反向依赖 Downloads。这意味着下载作为上层用例复用文件所有权和分享授权能力，而不是让基础领域适配下载模块。

## 组合根

[main.cpp](../src/main.cpp) 中的连接方式是：

~~~cpp
filelink::FileService fileService(mysqlPool);
filelink::FileApiRouter fileRouter(server, fileService, authService);
fileRouter.register_routes();
~~~

依赖显式可见，没有类在内部偷偷创建数据库连接、认证服务或其他 Router。

## 测试地图


| 测试                                                | 覆盖内容                                           |
| ----------------------------------------------------- | ---------------------------------------------------- |
| [FileHttpTests.cpp](../tests/FileHttpTests.cpp)         | file_id 路径解析、列表 JSON 和通用响应 |
| [FileServiceTest.cpp](../tests/FileServiceTest.cpp)     | 文件列表隔离、所有权删除和引用计数状态 |
| [DownloadServiceTest.cpp](../tests/DownloadServiceTest.cpp) | Downloads 通过 FileService 执行私有下载所有权检查 |
| [DownloadTest.sh](../tests/DownloadTest.sh)             | 真实服务中的 Files 列表、认证、删除和删除后下载失效 |

## 推荐代码阅读顺序

1. [FileApiRouter.cpp](../src/files/FileApiRouter.cpp)：先看列表和删除两个顶层流程。
2. [FileRequestParser.cpp](../src/files/FileRequestParser.cpp)：理解 URL 如何变成二进制 file_id。
3. [FileResponseView.cpp](../src/files/FileResponseView.cpp)：理解 JSON 和状态码如何输出。
4. [FileService.cpp](../src/files/FileService.cpp)：理解查询和删除事务。
5. [File.cpp](../src/database/File.cpp)：理解 files 表 SQL。
6. [Object.cpp](../src/database/Object.cpp)：理解删除逻辑文件时引用计数如何变化。

阅读时始终沿着这条主线：

~~~text
外部 HTTP 输入 → Parser / Auth → Router → FileService → Router → View → HTTP 输出
~~~
