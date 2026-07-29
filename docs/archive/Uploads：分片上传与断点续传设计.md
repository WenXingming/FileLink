# Uploads：分片上传与断点续传设计

在 FileLink 系统中，[src/uploads/](file:///home/wxm/FileLink/src/uploads/) 模块负责接收客户端上传的大文件。它完美实现了开放的断电续传协议 **TUS (tus.io 1.0.0 规范)**，并通过流式哈希缓存计算、并发锁定管理及最终原子发布机制，保障了超大文件在恶劣网络环境下的可靠传输。

本篇文档将详细剖析 `uploads` 模块的 TUS 状态机、API 路由逻辑、流式哈希器机制及具体的 C++ 代码实现。

---

## TUS 协议状态机与路由

TUS 协议的核心思想是通过一组特化的 HTTP 动作来控制上传生命周期：


| 动作与路由                 | 核心职责       | 状态机流转                                                                               |
| :--------------------------- | :--------------- | :----------------------------------------------------------------------------------------- |
| **POST `/uploads`**        | 初始化上传会话 | 创建`UPLOADING` 会话，并依据数据库 `objects` 状态尝试秒传；未命中时保留普通上传。        |
| **HEAD `/uploads/<id>`**   | 断点进度查询   | 返回当前服务端已持久化接收的`Upload-Offset` 与文件大小，用作客户端断网重连后的续传起点。 |
| **PATCH `/uploads/<id>`**  | 传输二进制分片 | 追加写入分片数据，更新哈希计算器，校验偏置并递增`committed_offset`。                     |
| **DELETE `/uploads/<id>`** | 取消上传会话   | 将 `UPLOADING` 会话原子标记为 `ABORTED`；`.part` 文件由离线 Cleaner 异步回收。            |

---

## 核心业务与流式哈希实现

`UploadService` 承担了高并发分片流式拼装、分片校验及物理落盘的工作。

### 创建会话与数据库秒传判定

在 [create_session](file:///home/wxm/FileLink/src/uploads/UploadService.cpp) 中，系统先创建 `UPLOADING` 会话，再以数据库 `objects` 表为业务事实来源尝试引用既有对象。数据库未命中、大小不匹配或对象处于 `RECLAIMING` 时，秒传失败只表示需要继续普通上传，不会把会话标记为失败：

```cpp
bool UploadService::create_session(const std::string& ownerUserId, uint64_t totalSize,
    const std::string& metadataHeader, const std::string& host, std::string& out_uploadIdHex) {
    const UploadMetadata metadata = parse_upload_metadata(metadataHeader);
    const std::string upload_id = generate_random_uuid_binary();
    const std::string expected_hash = metadata.expected_hash_hex.size() == 64
        ? hex_to_bytes(metadata.expected_hash_hex) : "";
    out_uploadIdHex = bytes_to_hex(upload_id);
    const db::UploadSession session = make_upload_session(upload_id, ownerUserId, totalSize,
        metadata, expected_hash);

    {
        db::SociSessionLease lease(pool_);
        db::UploadSessionDao(lease.get()).create(session);
    }

    if (!expected_hash.empty()) {
        complete_existing_session(upload_id, metadata.expected_hash_hex);
    }
    return true;
}
```

`complete_existing_session` 在一个 MySQL 事务中调用 `try_add_existing_reference`，只有哈希、大小和对象状态全部匹配时才增加引用、创建 `File`，并把 offset 更新到总大小后完成会话。它不查询对象路径，也不读取物理文件。

这里需要特别区分“秒传判定”和“秒传完成”：服务器确实只查询、修改数据库，不访问物理对象存储；但它不只是修改 `upload_sessions`。一次成功秒传会在同一个 MySQL 事务中完成三类业务数据变更：


| 数据表            | 秒传时的操作                                                                                    | 业务含义                                              |
| ------------------- | ------------------------------------------------------------------------------------------------- | ------------------------------------------------------- |
| `objects`         | 按`content_hash + byte_size` 原子增加 `ref_count`，必要时将 `PENDING_DELETE` 恢复为 `READY`     | 当前用户的新文件取得既有内容对象引用                  |
| `files`           | 插入一条属于当前用户的新 File                                                                   | 建立用户实际拥有的逻辑文件，而不是复用其他用户的 File |
| `upload_sessions` | 设置`committed_offset = total_size`、`state = COMPLETED`、`content_hash` 和 `completed_file_id` | 告诉客户端传输已经完成，并提供最终文件 ID             |

可以把服务端秒传理解为下面这笔数据库事务：

```text
读取 UPLOADING UploadSession
    ↓
条件更新 Object：哈希、大小和状态必须允许引用
    ├─ 更新不到记录：事务不完成，会话继续 UPLOADING
    └─ 引用成功：Object.ref_count + 1
                    ↓
                 创建 File
                    ↓
                 完成 UploadSession
                    ↓
                 提交事务
```

增加 Object 引用、创建 File 或完成 Session 中任何一步发生异常，整个事务都会回滚，不会留下“引用数增加但没有 File”或“Session 显示完成但没有最终文件”的半完成状态。因此，更准确的表述是：

> 服务器通过一笔数据库事务判定并完成秒传；它会协调 `objects`、`files` 和 `upload_sessions`，但不会读取或修改底层物理文件。

### 客户端如何识别秒传

`POST /uploads` 的 `201 Created` 只表示会话创建成功，`Location` 响应头给出新会话地址。它不表示客户端可以假定 offset 为 0，因为服务端可能已经在创建过程中完成数据库秒传。因此，无论是新建会话还是恢复本地保存的会话，客户端都必须先发送一次 `HEAD`，以服务端返回的 `Upload-Offset` 为准：

```text
客户端                              服务端                         MySQL
  │                                  │                              │
  │ POST /uploads                    │                              │
  │ Upload-Length: N                 │                              │
  │ Upload-Metadata: expected_hash H │                              │
  ├─────────────────────────────────►│ 创建 UploadSession           │
  │                                  ├─────────────────────────────►│ UPLOADING, offset=0
  │                                  │                              │
  │                                  │ 尝试引用既有 Object(H, N)    │
  │                                  ├─────────────────────────────►│
  │                                  │      ┌───────────────────────┤
  │                                  │      │ 命中：Object +1、创建 File、
  │                                  │      │ offset=N、COMPLETED
  │                                  │      │
  │                                  │      │ 未命中：保持 UPLOADING、offset=0
  │                                  │◄─────┴───────────────────────┤
  │ 201 Created + Location           │                              │
  │◄─────────────────────────────────┤                              │
  │                                  │                              │
  │ HEAD <Location>                  │                              │
  ├─────────────────────────────────►│ 查询数据库中的会话进度         │
  │◄─────────────────────────────────┤                              │
  │ Upload-Offset: 0 或 N            │                              │
```

客户端根据 `HEAD` 结果进入两条互斥路径：

```text
Upload-Offset == Upload-Length
    → 数据库秒传已完成
    → 不发送 PATCH
    → GET 会话状态，取得 file_id

Upload-Offset < Upload-Length
    → 从 Upload-Offset 开始发送 PATCH
    → 每次使用服务器返回的新 offset 发送下一片
    → 最后一片完成后轮询 GET，等待 COMPLETED
```

网页端对应实现位于 `web/static/js/app.js`。新会话创建后立即同步 offset，而不是写死为 0：

```js
sessionUrl = await createTusSession(file);
localStorage.setItem(fingerprint, sessionUrl);
offset = await getTusSessionOffset(sessionUrl);
```

若客户端忽略 `HEAD`，在秒传完成后仍发送 `PATCH offset=0`，服务端会发现数据库中的 `committed_offset` 已经等于总大小并返回 `409 Offset Mismatch`。这个 409 是必要的并发和偏移保护，但不应作为正常的秒传通知机制；正常流程应通过 `HEAD` 主动同步服务端进度。

当前网页端尚未计算文件的 BLAKE3，也没有在 `Upload-Metadata` 中发送 `expected_hash`，所以它暂时只会走普通上传。上述 `HEAD` 流程使客户端协议已经能够正确处理秒传；使用支持 `expected_hash` 的客户端时，命中后会直接跳过全部 `PATCH`。

### 分片追加写入与流式哈希更新

在 [write_session_chunk](file:///home/wxm/FileLink/src/uploads/UploadService.cpp#L216) 中，为了防止高频 PATCH 分片时重复读取整文件重新计算哈希带来的巨大 I/O 损耗，系统采用了一个内存哈希缓存区（`ActiveHasher`）。当发生缓存失效（如服务重启）时，可按需回读重构状态：

```cpp
UploadChunkResult UploadService::write_session_chunk(const std::string& ownerUserId,
    const std::string& uploadIdHex, uint64_t clientOffset, const std::string& chunkData,
    uint64_t& out_newOffset) {
    std::string uploadIdBinary = parse_upload_id_to_binary(uploadIdHex);

    try {
        db::SociSessionLease lease(pool_);
        soci::session& sql = lease.get();
        soci::transaction tr(sql); // 开启事务锁定会话

        db::UploadSessionDao sessionStore(sql);
        db::UploadSession session;
        if (!sessionStore.find(uploadIdBinary, session) || session.owner_user_id != ownerUserId) {
            return UploadChunkResult::SessionNotFound;
        }

        // 校验客户端上传偏移量是否连续
        UploadChunkResult validation = validate_session_offset(session, clientOffset, chunkData.size());
        if (validation != UploadChunkResult::Success) { return validation; }

        // 1. 将分片数据物理追加写入到临时 `.part` 文件中
        if (!write_chunk_to_file(uploadIdHex, clientOffset, chunkData)) {
            return UploadChunkResult::SystemError;
        }

        const uint64_t nextOffset = clientOffset + chunkData.size();
        const bool isComplete = (nextOffset == session.total_size);
  
        // 2. 更新内存流式哈希器并计算哈希值
        std::string finalHash;
        try {
            finalHash = update_stream_hash(uploadIdHex, get_part_file_path(uploadIdHex), clientOffset, chunkData, isComplete);
        } catch (...) {
            return UploadChunkResult::SystemError;
        }

        // 3. 更新数据库中的已提交偏移量
        sessionStore.update_offset(uploadIdBinary, nextOffset);
  
        // 4. 若传输完毕，将会话置为 FINALIZING 状态，以防止并发 PATCH 冲突
        if (isComplete) {
            sessionStore.update_state(uploadIdBinary, "FINALIZING");
        }
  
        tr.commit(); // 提交事务
        out_newOffset = nextOffset;

        // 5. 如果文件已全部收齐，在事务外异步或同步触发物理对象原子提交与发布
        if (isComplete) {
            finalize_session(uploadIdHex, finalHash);
        }

        return UploadChunkResult::Success;
    } catch (...) {
        return UploadChunkResult::SystemError;
    }
}
```

### 完成上传（文件入库发布）

在 [finalize_session](file:///home/wxm/FileLink/src/uploads/UploadService.cpp#L315) 中，完成最后的一致性检验并交付 `ObjectStore` 原子提交：

```cpp
void UploadService::finalize_session(std::string uploadIdHex, std::string realHashHex) {
    std::string uploadIdBinary = parse_upload_id_to_binary(uploadIdHex);
    std::string partPath = get_part_file_path(uploadIdHex);

    // 1. 若中途哈希意外丢失，回读文件重新计算 BLAKE3 哈希值作为兜底
    if (realHashHex.empty()) {
        if (!compute_file_hash(partPath, realHashHex)) {
            mark_session_failed(uploadIdBinary, "Hashing failed");
            return;
        }
    }

    // 2. 与客户端在初始化元数据中声明的预期哈希进行一致性比对 (防网络篡改)
    if (!verify_expected_hash(uploadIdBinary, realHashHex)) {
        ::unlink(partPath.c_str());
        return;
    }

    // 3. 提交至物理 ObjectStore，在文件系统层面执行原子硬链接/去重操作
    if (!commit_to_object_store(partPath, realHashHex)) {
        mark_session_failed(uploadIdBinary, "Failed to commit to store");
        return;
    }

    // 4. 在 MySQL 本地事务中：增加 objects 引用数，向 files 表写入记录，并将会话标记为完成
    if (!complete_newly_published_session(uploadIdBinary, realHashHex)) {
        mark_session_failed(uploadIdBinary, "Failed to create logical file");
    }
}
```
