# 上传异常、超时与进程崩溃恢复机制

为了保证大文件在网络异常、服务器崩溃等极限情况下的数据完整性与可靠性，FileLink 结合 Tus 协议设计了一套完备的异常处理与自愈恢复机制。本文档详细记录了客户端预期哈希的生成来源，以及系统在上传失败、会话超时和进程崩溃时的处理措施。

---

## 1. 客户端预期哈希 (expected_hash) 的来源

客户端预期的 `expected_hash` 是在**上传发起前，由客户端在本地计算生成的**：

1. **本地哈希计算**：
   * 在上传开始前，客户端（如浏览器端的 JavaScript、WASM 或命令行 CLI 工具）会首先读取本地的完整文件，在本地计算出文件的 **BLAKE3 哈希值**（64 位小写十六进制字符串）。
2. **元数据提交**：
   * 根据 Tus 协议规范的 `Creation` 扩展，客户端发起 `POST /uploads` 创建上传会话时，会通过 HTTP 请求头 `Upload-Metadata` 提交该哈希。
   * 例如，哈希值通过 Base64 编码后拼接在头部：`Upload-Metadata: expected_hash dGVzdF9oYXNoX3ZhbHVl...`。
3. **哈希作用**：
   * 该预期的哈希随会话持久化在 MySQL 中，主要用于**哈希秒传**（与对象库中的已有文件比对）和**终结校验**（文件上传完毕后校验数据是否损坏）。

---

## 2. 上传失败的处理机制 (Upload Failure)

在发生网络中断、写入失败或哈希不匹配等上传失败场景下，系统通过以下措施进行保护：

* **哈希不匹配的物理清理**：
  在最后一个分片上传并触发异步终结（`finalize_session`）时，若计算出的文件哈希与客户端 `expected_hash` 不一致：
  * 服务端会立即调用 `::unlink` **物理删除**磁盘上的 `.part` 临时分片文件，防止垃圾文件长期占用磁盘空间。
  * 将数据库中该会话的 `state` 置为 `FAILED`，并在 `failure_reason` 列中记录 `"BLAKE3 checksum mismatch"` 供客户端排查。
* **分片写入失败与事务回滚**：
  * 在 [write_session_chunk](file:///home/wxm/FileLink/src/UploadService.cpp#L198) 的分片写入过程中，一旦遇到任何系统报错或抛出异常，整个 MySQL 数据库事务会自动**全回滚**，确保数据库中的 `committed_offset` 进度绝不超前。
  * 同时，服务端在 `catch(...)` 块中会立即**清除该会话在内存中的 Hasher 缓存**（从 `activeHashers_` 中擦除），防止已被踩坏的错误状态遗留在内存中。下次客户端重试时会重新触发重建。

---

## 3. 超时处理机制 (Expiration)

针对长时间被放弃的上传会话，系统在**数据库**和**内存**两个层面上防止资源泄露：

* **MySQL 级的会话过期**：
  * 在 [create_session](file:///home/wxm/FileLink/src/UploadService.cpp#L137) 阶段，会话被赋予默认 **24 小时** 的有效期（`session.expires_at = now + 24h`）。
  * 数据库的清理脚本会定期扫描并将过期会话的状态置为 `EXPIRED`，随后统一回收其占用的磁盘空间。
* **内存 Hasher 缓存清理**：
  * 为防止内存中的 `activeHashers_` 持续增长，在加锁更新分片时，若 Map 大小超过限制（如 128），将触发 [clean_expired_hashers_under_lock](file:///home/wxm/FileLink/src/UploadService.cpp#L457)。
  * 系统会扫描并擦除超过 **1 小时** 未发生数据追加更新的会话 Hasher。即使因为超时被误删，由于系统具备自愈重建能力，下次客户端续传时也能无缝恢复。

---

## 4. 进程崩溃恢复机制 (Process Crash Recovery)

如果服务器发生硬件断电、进程崩溃（Crash）或突然重启，系统基于 **物理落盘 + 延迟重建** 能够实现零数据损坏的自愈恢复：

```text
[ 进程崩溃前 ]
分片写入 ──> fdatasync(落盘) ──> 事务提交(MySQL Offset 递增)
(确保“已落盘数据”与“数据库进度”原子一致)

[ 进程崩溃重启后 ]
客户端发起 HEAD ──> 查询数据库最新 Offset ──> 客户端从断点续传 PATCH
                                                      │
                                                      ▼
                                       服务端发现无内存 Hasher
                                       触发 Lazy Reconstruction (延迟重建)
                                       从头读取磁盘上已落盘的前缀字节
                                       还原 Hasher 状态 ──> 继续流式计算 Hash
```

1. **进度与数据原子绑定**：
   * 写入分片数据后，服务端先执行 `::fdatasync(fd)` 保证操作系统缓存中的数据强制落盘，防止断电时文件数据丢失或产生空洞。
   * 紧接着在同一个数据库事务中更新 `committed_offset = newOffset`。只有当数据落盘与数据库更新都成功时，才向客户端承诺进度，保证了崩溃时进度数据的 100% 可信度。
2. **内存状态的延迟重建 (Lazy Reconstruction)**：
   * 进程重启后，内存中的 Map 缓存完全丢失。
   * 客户端重试时，会先通过 `HEAD /uploads/{id}` 查询到最后持久化的 `committed_offset`（如 10MB），然后从 `10MB` 处继续 PATCH 数据。
   * 服务端在检测到 `clientOffset = 10MB > 0` 且内存中找不到 Hasher 时，会主动调用 [reconstruct_hasher_from_file](file:///home/wxm/FileLink/src/UploadService.cpp#L433)，打开磁盘上的 `.part` 文件，从头读取已落盘的 `10MB` 字节数据，喂入重新初始化的 Hasher，从而在内存中完全**还原崩溃前的 Hasher 状态**。
   * 还原完成后，新到来的 PATCH 字节得以顺利无缝地追加更新。客户端无需重传已写入的数据，即以**零开销**完成了崩溃恢复。
