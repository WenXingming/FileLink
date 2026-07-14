# Uploads：分片上传与断点续传设计

在 FileLink 系统中，[src/uploads/](file:///home/wxm/FileLink/src/uploads/) 模块负责接收客户端上传的大文件。它完美实现了开放的断电续传协议 **TUS (tus.io 1.0.0 规范)**，并通过流式哈希缓存计算、并发锁定管理及最终原子发布机制，保障了超大文件在恶劣网络环境下的可靠传输。

本篇文档将详细剖析 `uploads` 模块的 TUS 状态机、API 路由逻辑、流式哈希器机制及具体的 C++ 代码实现。

---

## TUS 协议状态机与路由

TUS 协议的核心思想是通过一组特化的 HTTP 动作来控制上传生命周期：

| 动作与路由 | 核心职责 | 状态机流转 |
| :--- | :--- | :--- |
| **POST `/uploads`** | 初始化上传会话 | 解析元数据，查磁盘校验预秒传，生成会话并置为 `UPLOADING` 或直接 `FINALIZING` 完成秒传。 |
| **HEAD `/uploads/<id>`** | 断点进度查询 | 返回当前服务端已持久化接收的 `Upload-Offset` 与文件大小，用作客户端断网重连后的续传起点。 |
| **PATCH `/uploads/<id>`** | 传输二进制分片 | 追加写入分片数据，更新哈希计算器，校验偏置并递增 `committed_offset`。 |
| **DELETE `/uploads/<id>`** | 取消上传会话 | 销毁上传任务，并立即擦除磁盘上未拼接完的临时 `.part` 文件。 |

---

## 核心业务与流式哈希实现

`UploadService` 承担了高并发分片流式拼装、分片校验及物理落盘的工作。

### 创建会话与预秒传判定

在 [create_session](file:///home/wxm/FileLink/src/uploads/UploadService.cpp#L192) 中，系统解析 `Upload-Metadata`，并先通过物理磁盘 `stat` 判定是否能够零字节瞬时秒传：

```cpp
bool UploadService::create_session(const std::string& ownerUserId, uint64_t totalSize,
    const std::string& metadataHeader, const std::string& host, std::string& out_uploadIdHex) {
    const UploadMetadata metadata = parse_upload_metadata(metadataHeader);
    const std::string upload_id = generate_random_uuid_binary();
    const std::string expected_hash = metadata.expected_hash_hex.size() == 64
        ? hex_to_bytes(metadata.expected_hash_hex) : "";
    
    // 1. 直查物理磁盘，判断该哈希对象是否已完整存在
    const bool is_deduplicated = object_matches_upload(store_, metadata.expected_hash_hex, totalSize);

    out_uploadIdHex = bytes_to_hex(upload_id);
    const db::UploadSession session = make_upload_session(upload_id, ownerUserId, totalSize,
        metadata, expected_hash, is_deduplicated);

    {
        db::SociSessionLease lease(pool_);
        db::UploadSessionDao(lease.get()).create(session);
    }

    // 2. 如果命中秒传，直接绕过物理传输阶段，在事务中创建逻辑文件并完成会话
    if (is_deduplicated && !complete_published_session(upload_id, metadata.expected_hash_hex)) {
        mark_session_failed(upload_id, "Failed to create logical file");
        return false;
    }
    return true;
}
```

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
    if (!complete_published_session(uploadIdBinary, realHashHex)) {
        mark_session_failed(uploadIdBinary, "Failed to create logical file");
    }
}
```
