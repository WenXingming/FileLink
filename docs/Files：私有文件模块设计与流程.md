# Files：私有文件模块设计与流程

本文对应当前 `src/files/` 实现，帮助读者理解用户如何查看、下载和删除自己拥有的逻辑文件。

## 一句话概括

Files 模块管理用户拥有的逻辑文件记录：`FileApiRouter` 处理 HTTP 请求，`FileService` 查询或删除数据库记录，`ObjectStore` 只把内容哈希转换为对象路径，真正的文件字节由 Nginx 发送。

## 模块地图

Files HTTP 流程采用轻量的解析器、Controller、Service、View 分工：

```text
HttpRequest
    │
    ▼
FileRequestParser   解析文件路径和二进制 ID
    │
    ▼
FileApiRouter       Controller：认证并编排一次 HTTP 请求
    │
    ▼
FileService         Model / Application Service：查询或删除逻辑文件
    │
    ▼
FileResponseView    View：构造文件列表、删除和错误响应
    │
    ▼
HttpResponse
```

| 文件 | 单一职责 |
| --- | --- |
| [`FileRequestParser`](../src/files/FileRequestParser.h) | 解析 `/files` 路径中的十六进制文件 ID |
| [`FileApiRouter`](../src/files/FileApiRouter.h) | 注册文件路由，编排解析、认证、Service 和 View |
| [`FileService`](../src/files/FileService.h) | 查询用户文件，并维护删除文件的数据库事务 |
| [`FileResponseView`](../src/files/FileResponseView.h) | 构造文件列表、删除成功和错误响应 |
| [`ObjectStore`](../src/storage/ObjectStore.h) | 根据内容哈希定位物理对象，不负责用户权限 |

模块没有文件仓库接口、工厂或 DI 容器。`main.cpp` 创建具体对象，并通过构造函数引用把它们连接起来。

## HTTP 接口

| 方法与路径 | 成功响应 | 作用 |
| --- | --- | --- |
| `GET /files` | `200 OK` | 列出当前用户拥有的文件 |
| `GET /files/{file_id}/download` | `200 OK` + `X-Accel-Redirect` | 下载当前用户拥有的文件 |
| `DELETE /files/{file_id}` | `204 No Content` | 删除逻辑文件并减少对象引用 |

`file_id` 在 URL 中是 32 位小写十六进制文本，对应数据库中的 16 字节二进制 ID。格式错误、文件不存在和文件不属于当前用户均返回 `404`，不会泄露其他用户的文件是否存在。

## 请求解析：FileRequestParser

[`FileRequestParser.cpp`](../src/files/FileRequestParser.cpp) 只识别两种固定路径：

```text
/files/{32 位小写十六进制 ID}
/files/{32 位小写十六进制 ID}/download
```

Parser 校验完整路径并把十六进制 ID 解码为 16 字节字符串。它不读取 Cookie、不查询数据库，也不决定解析失败应该返回哪个状态码。

## Controller：FileApiRouter

[`FileApiRouter.cpp`](../src/files/FileApiRouter.cpp) 是 Files 模块的 HTTP 入口。每个 handler 都保持相同的阅读顺序：

```text
1. 解析路径
2. 认证当前用户
3. 调用 FileService
4. 选择响应 View
```

`GET /files` 没有路径参数，因此直接从认证开始；下载和删除则先拒绝格式错误的路径，再认证用户。

### `/files/` 前缀为什么还会经过 Shares

Tudou 当前按前缀注册路由，而下面两组接口共享 `/files/`：

```text
/files/{file_id}
/files/{file_id}/download
/files/{file_id}/shares
/files/{file_id}/shares/{share_id}
```

因此 `FileApiRouter` 是这个前缀的唯一入口，并优先询问 `ShareApiRouter` 请求是否属于分享管理接口。若不是，才按 HTTP 方法进入私有下载或删除流程。这是路由框架边界，不表示 Files 拥有分享业务。

## 列表流程

```text
解析 Session Cookie 并认证用户
    ▼
FileService::list_files(owner_user_id)
    ▼
FileDao::find_by_owner
    ▼
FileResponseView::file_list
```

成功响应只暴露客户端需要的字段：

```json
{
  "files": [
    {
      "file_id": "616c6963652d66696c652d6964303031",
      "name": "report.pdf"
    }
  ]
}
```

数据库中的 `owner_user_id`、`content_hash` 和时间字段不会出现在响应里。

## 私有下载流程

```text
解析 file_id
    ▼
认证当前用户
    ▼
FileService::find_file(owner_user_id, file_id)
    ▼
ObjectStore::get_object_key(content_hash)
    ▼
返回 X-Accel-Redirect
    ▼
Nginx 从对象目录发送文件字节
```

`FileService::find_file` 使用文件 ID 和用户 ID 联合查询，因此所有权检查与文件查询是同一个数据库操作。

应用返回的响应体为空，关键 Header 类似：

```http
HTTP/1.1 200 OK
Content-Type: application/octet-stream
Content-Disposition: attachment; filename="report.pdf"
X-Accel-Redirect: /_filelink_objects/aa/bb/<content-hash>
```

FileLink 进程只处理元数据和内部路径，不把文件内容读入 `std::string`，也不通过应用层 Socket 循环发送文件。Nginx 接管内部重定向并发送文件，因此应用内存不会随下载文件大小线性增长。

## 删除流程与事务边界

删除逻辑文件不等于立刻删除磁盘对象。一个物理对象可能被多个逻辑文件引用，因此 [`FileService::delete_file`](../src/files/FileService.cpp) 在同一个数据库事务中执行：

```text
按 file_id + owner_user_id 查询文件
    ▼
删除 files 记录
    ▼
减少 objects.ref_count
    ▼
提交事务
```

如果逻辑文件不存在或不属于当前用户，事务不提交并返回 `404`。如果逻辑文件引用的对象记录意外缺失，Service 抛出异常，SOCI 事务析构时回滚，Router 返回 `500`。

当对象引用数降为零时，它只进入待回收状态；磁盘对象由 Cleaner 在离线流程中安全删除。这样在线删除请求不需要承担物理文件删除失败和并发复用问题。

## 三种“文件”不要混淆

| 概念 | 保存位置 | 含义 |
| --- | --- | --- |
| 逻辑文件 `files` | MySQL | 用户可见的名称、所有者和内容引用 |
| 物理对象 `objects` | MySQL + 存储目录 | 按内容哈希去重后的真实字节对象 |
| 上传会话 `upload_sessions` | MySQL + 临时目录 | 尚未完成或正在收尾的分片上传过程 |

Files 模块主要管理第一种。它通过 `content_hash` 引用第二种，但不管理上传会话，也不直接传输物理对象内容。

## 与相邻模块的边界

```text
uploads ──完成上传──► files ──content_hash──► storage
                         │
                         └──file_id──► shares
```

- Uploads 决定何时创建逻辑文件。
- Files 管理用户自己的逻辑文件生命周期和私有下载授权。
- Shares 管理针对某个逻辑文件的公开访问授权。
- Storage 管理内容寻址对象的发布和定位。
- Cleaner 回收已经没有引用的物理对象。

当前没有独立的 Downloads 业务模块，因为下载没有会话或状态机。私有下载只是 Files 的一个用例，实际字节传输由 Nginx 完成。

