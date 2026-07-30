# Shares：分享模块设计与流程

本文对应当前 <code>src/shares/</code> 实现，目标是帮助读者理解文件所有者如何创建、查看和撤销分享，以及公开 Token 如何被 Downloads 模块验证。

## 一句话概括

Shares 模块管理“谁可以在什么时间内公开访问某个逻辑文件”：浏览器只在创建成功时拿到原始 Token，MySQL 只保存 Token 哈希，<code>ShareService</code> 负责所有权、有效期和撤销状态。

## 模块地图

分享管理接口采用 Parser、Controller、Service、View 分工：

~~~text
HttpRequest
    │
    ▼
ShareRequestParser  解析 file_id、share_id 和有效期
    │
    ▼
ShareApiRouter      Controller：认证并编排创建、列表或撤销
    │
    ▼
ShareService        Model / Application Service：管理分享授权
    │
    ▼
ShareResponseView   View：构造状态码和 JSON
    │
    ▼
HttpResponse
~~~

公开下载不会进入 <code>ShareApiRouter</code>。Downloads 模块只复用 ShareService 的 Token 查询能力：

~~~text
GET /downloads/shared/{token}
    │
    ▼
DownloadApiRouter
    │
    ▼
DownloadService
    │
    ▼
ShareService::find_shared_file
    │
    ▼
有效的 db::File 或 Not Found
~~~

| 文件 | 单一职责 |
| --- | --- |
| [<code>ShareRequestParser</code>](../src/shares/ShareRequestParser.h) | 解析分享管理路径和 <code>expires_in_seconds</code> |
| [<code>ShareApiRouter</code>](../src/shares/ShareApiRouter.h) | 注册分享管理端点并编排 Parser、认证、Service 和 View |
| [<code>ShareService</code>](../src/shares/ShareService.h) | 验证文件所有权，生成、查询和撤销分享授权 |
| [<code>ShareResponseView</code>](../src/shares/ShareResponseView.h) | 构造创建、列表、撤销和错误响应 |
| [<code>ShareDao</code>](../src/database/Share.h) | 执行 shares 表的创建、有效查询和软撤销 SQL |

模块没有分享 Repository 接口、策略类、工厂或 DI 容器。<code>main.cpp</code> 创建具体的 ShareService 并注入 Router 和 DownloadService。

## HTTP 接口

| 方法与路径 | 输入 | 成功响应 | 作用 |
| --- | --- | --- | --- |
| <code>POST /shares/{file_id}</code> | Session Cookie、file_id、有效期 JSON | <code>201 Created</code> | 为自己的文件创建分享 |
| <code>GET /shares/{file_id}</code> | Session Cookie、file_id | <code>200 OK</code> | 列出文件仍然有效的分享 |
| <code>DELETE /shares/{file_id}/{share_id}</code> | Session Cookie、两个 ID | <code>204 No Content</code> | 撤销指定分享 |

公开下载使用独立的 Downloads 接口：

~~~text
GET /downloads/shared/{token}
~~~

三个模块使用不同的顶层前缀：

~~~text
/files/...       逻辑文件管理
/shares/...      分享授权管理
/downloads/...   文件交付
~~~

因此 Tudou 不需要参数路由，也不需要让 FileApiRouter 转发 Shares 请求。每个 Controller 只注册并理解自己的前缀。

## 请求解析层：ShareRequestParser

[<code>ShareRequestParser.cpp</code>](../src/shares/ShareRequestParser.cpp) 识别两种路径：

~~~text
/shares/{32 位小写十六进制 file_id}
/shares/{32 位小写十六进制 file_id}/{32 位小写十六进制 share_id}
~~~

Parser 校验完整路径，并把两个 ID 解码为数据库使用的 16 字节二进制字符串。

创建分享时，Parser 还从 JSON Body 读取：

~~~json
{"expires_in_seconds": 3600}
~~~

有效期必须：

- 是 JSON 整数；
- 大于 0；
- 不超过 30 天；
- 与当前时间相加后不溢出 <code>time_t</code>。

Parser 最终把相对秒数转换为绝对过期时间。它不验证文件所有权，也不写数据库。

## Controller：ShareApiRouter

[<code>ShareApiRouter.cpp</code>](../src/shares/ShareApiRouter.cpp) 只持有：

~~~text
HttpServer
ShareService
AuthService
~~~

<code>register_routes</code> 直接根据路径形状区分集合与成员，再根据 HTTP 方法选择用例：

~~~text
/shares/{file_id}
    ├── POST   ─► handle_create
    ├── GET    ─► handle_list
    └── 其他   ─► 404

/shares/{file_id}/{share_id}
    ├── DELETE ─► handle_revoke
    └── 其他   ─► 404
~~~

每个 handler 都遵循相同顺序：

~~~text
解析输入 → 认证用户 → 调用 ShareService → 选择 ShareResponseView
~~~

Router 不生成随机 Token、不计算 Token 哈希，也不执行 SQL。

## 创建分享流程

~~~text
POST /shares/{file_id}
    ▼
解析 file_id 和 expires_in_seconds
    ├── 输入非法 ─► 400 Bad Request
    └── 成功
          ▼
认证当前用户
    ├── 失败 ─► 401 / 500
    └── 成功
          ▼
ShareService::create_share
    ▼
按 file_id + owner_user_id 验证文件所有权
    ├── 不存在或不属于用户 ─► 404
    └── 成功
          ▼
生成 share_id 与原始 Token
    ▼
数据库保存 Token 哈希和过期时间
    ▼
返回 201 + share_id + 原始 Token + expires_at
~~~

成功响应示例：

~~~json
{
  "share_id": "73686172652d6170692d696430303031",
  "token": "<64 位小写十六进制 Token>",
  "expires_at": 1700000000
}
~~~

原始 Token 只在创建成功的响应中出现一次。列表接口不会再次返回 Token。

## Token 的生成与保存

创建分享时，ShareService：

1. 使用 libsodium 生成 32 字节随机 Token。
2. 使用 libsodium 通用哈希计算 32 字节 <code>token_hash</code>。
3. 生成独立的 16 字节 <code>share_id</code>。
4. 在 MySQL 中保存 share_id、file_id、token_hash 和 expires_at。
5. 把原始 Token 编码成 64 位十六进制文本返回。

数据分工如下：

| 位置 | 保存内容 | 用途 |
| --- | --- | --- |
| 分享 URL | 原始 Token 的十六进制文本 | 匿名访问凭证 |
| MySQL <code>shares.token_hash</code> | 原始 Token 的哈希 | 查找授权，不保存可直接重放的凭证 |
| MySQL <code>shares.share_id</code> | 分享记录 ID | 文件所有者查看和撤销分享 |

<code>share_id</code> 是管理标识，<code>token</code> 是公开访问凭证，两者不能互换。

## 列出分享流程

~~~text
GET /shares/{file_id}
    ▼
解析 file_id 并认证用户
    ▼
ShareService::list_shares(owner_user_id, file_id)
    ▼
验证文件属于当前用户
    ├── 否 ─► 404
    └── 是
          ▼
ShareDao::find_active_by_file_id
          ▼
只返回 revoked_at IS NULL 且 expires_at > NOW() 的分享
~~~

响应示例：

~~~json
{
  "shares": [
    {
      "share_id": "73686172652d6170692d696430303031",
      "expires_at": 1700000000
    }
  ]
}
~~~

响应不包含 <code>token_hash</code>，也不能恢复原始 Token。

## 撤销分享流程

~~~text
DELETE /shares/{file_id}/{share_id}
    ▼
解析两个 ID 并认证用户
    ▼
验证 file_id 属于当前用户
    ├── 否 ─► 404
    └── 是
          ▼
UPDATE shares SET revoked_at = NOW()
WHERE share_id = ? AND file_id = ? AND revoked_at IS NULL
    ├── 未更新 ─► 404
    └── 成功   ─► 204
~~~

撤销是软删除：数据库保留分享记录，但所有有效查询都会排除 <code>revoked_at</code> 非空的记录。

## 公开 Token 验证

<code>ShareService::find_shared_file</code> 是 Shares 暴露给 Downloads 的最小业务能力：

~~~text
64 位十六进制 Token
    ▼
解码为 32 字节原始 Token
    ├── 格式非法 ─► false
    └── 成功
          ▼
计算 token_hash
    ▼
查询未撤销且未过期的 Share
    ├── 不存在 ─► false
    └── 存在
          ▼
按 share.file_id 查询 db::File
          ▼
返回文件元数据
~~~

它不构造 <code>HttpResponse</code>，也不访问 ObjectStore。Downloads 拿到文件元数据后才负责定位物理对象。

## View：ShareResponseView

[<code>ShareResponseView.cpp</code>](../src/shares/ShareResponseView.cpp) 集中维护：

- <code>created</code>：返回 <code>201</code> 和创建结果。
- <code>share_list</code>：返回有效分享数组。
- <code>revoked</code>：返回 <code>204</code>。
- <code>invalid_expiry</code>：返回 <code>400</code>。
- <code>unauthorized</code>、<code>not_found</code>、<code>server_error</code>：构造统一 JSON 错误。

View 负责把二进制 share_id 编码为十六进制，并把 <code>std::tm</code> 转换为 Unix 时间戳。它不验证分享状态。

## 模块边界与依赖方向

~~~text
FileApiRouter      ShareApiRouter      DownloadApiRouter
      │                   │                    │
      ▼                   ▼                    ▼
 FileService         ShareService ◄──── DownloadService
      ▲                                      │
      └──────────────────────────────────────┘
~~~

- FileService 负责文件所有权。
- ShareService 负责分享授权。
- DownloadService 组合这两种能力并负责对象定位。
- ShareApiRouter 不处理匿名下载。
- Files 与 Shares Controller 之间没有依赖。

## 组合根

[<code>main.cpp</code>](../src/main.cpp) 显式连接这些对象：

~~~cpp
filelink::ShareService shareService(mysqlPool);
filelink::ShareApiRouter shareRouter(server, shareService, authService);
shareRouter.register_routes();

filelink::DownloadService downloadService(fileService, shareService);
~~~

同一个 ShareService 实例同时服务于分享管理和分享下载授权，但两个 Router 仍各自维护自己的 HTTP 协议。

## 测试地图

| 测试 | 覆盖内容 |
| --- | --- |
| [<code>ShareHttpTests.cpp</code>](../tests/ShareHttpTests.cpp) | 分享路径、有效期解析、创建和列表响应 |
| [<code>ShareServiceTest.cpp</code>](../tests/ShareServiceTest.cpp) | 创建、有效查询、过期和撤销规则 |
| [<code>DownloadServiceTest.cpp</code>](../tests/DownloadServiceTest.cpp) | 有效 Token 查询以及撤销后失效 |
| [<code>DownloadTest.sh</code>](../tests/DownloadTest.sh) | 真实服务中的分享创建、列表、认证、下载和撤销 |

## 推荐代码阅读顺序

1. [<code>ShareApiRouter.cpp</code>](../src/shares/ShareApiRouter.cpp)：先看创建、列表和撤销的顶层流程。
2. [<code>ShareRequestParser.cpp</code>](../src/shares/ShareRequestParser.cpp)：理解路径和有效期如何解析。
3. [<code>ShareResponseView.cpp</code>](../src/shares/ShareResponseView.cpp)：理解公开 JSON 格式。
4. [<code>ShareService.cpp</code>](../src/shares/ShareService.cpp)：理解所有权、Token 和撤销规则。
5. [<code>Share.cpp</code>](../src/database/Share.cpp)：理解有效分享 SQL。
6. [<code>DownloadService.cpp</code>](../src/downloads/DownloadService.cpp)：理解分享授权如何进入下载流程。

阅读分享管理时沿着：

~~~text
HTTP 输入 → ShareRequestParser → ShareApiRouter → ShareService → ShareResponseView
~~~

阅读公开下载时沿着：

~~~text
Token → DownloadService → ShareService → db::File → ObjectStore::get_object_key
~~~
