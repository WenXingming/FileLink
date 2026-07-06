# Tus 可恢复上传与技术选型设计

本篇文档记录了 FileLink 中用于支持大文件断点续传的 **Tus 协议实现** 以及背后的 **技术选型决策**，供团队成员查阅与面试准备。

---

## 1. 业务场景与架构设计

FileLink 作为团队内部的模型权重、数据集和安装包分发平台，需要处理数 GB 至数十 GB 的超大文件。为此，上传链路必须满足以下要求：
1. **流式处理**：不能将整个大文件读入内存。
2. **断点续传**：支持网络抖动、浏览器刷新或服务端重启后的无缝恢复。
3. **强一致性**：重试或并发请求不能造成数据块乱序或重复写入。
4. **去重秒传**：上传前能够基于文件 Hash 校验实现瞬间秒传。

基于上述需求，FileLink 服务端实现了一套**兼容 Tus 1.0 规范的最小服务端**，其上传生命周期与架构如下：

```text
       [ 1. 校验秒传 (BLAKE3) ]
客户端 ───────────────────────────> preflight (秒传成功 -> 逻辑关联完成)
  │ (秒传失败)
  ▼
[ 2. 创建会话 (Tus POST) ]
客户端 ───────────────────────────> Server (生成 UploadID, 写入 MySQL)
  │ (返回 201 Created & Location)
  ▼
[ 3. 查询进度 (Tus HEAD) ]
客户端 ───────────────────────────> Server (查询 MySQL 获取 committed_offset)
  │ (返回当前已落盘的字节数 Offset)
  ▼
[ 4. 传输数据 (Tus PATCH) ]
客户端 ───────────────────────────> Server (检验 Offset 并顺序追加写入 .part 文件)
  │ (返回 204 No Content & 最新 Offset)
  ▼ (当已上传字节 == 文件总大小)
[ 5. 异步落盘与发布 ]
Server (后台线程计算完整文件 BLAKE3 -> 移动到 ObjectStore 进行原子去重 -> 标记 COMPLETED)
```

---

## 2. 核心技术选型决策

在方案设计阶段，我们对业内多种流式/分片上传方案进行了深度对比，最终做出了最适合单机内容寻址架构的决策。

### 决策 1：为什么选择 Tus 协议，而不是自定义私有分片协议？
* **方案对比**：
  * *自定义协议*：需要在前端手写切块、并发控制、错误重试、LocalStorage 记录等复杂逻辑，容易出现边界情况 Bug。
  * *Tus 协议*：是一个开源的 HTTP 续传标准规范，前端可以直接复用成熟的官方客户端 **`tus-js-client`**。
* **选型结论**：**采用 Tus 协议标准**。
  * 我们通过兼容 Tus 规范，成功将前端复杂的切片、退避重试和断点记录“外包”给了成熟的客户端，服务端仅需实现最简单的 HTTP 头部解析和状态流转，大幅降低了系统总复杂度。

### 决策 2：为什么采用嵌入式最小实现，而不是引入官方的 `tusd` 独立服务（Sidecar）？
* **方案对比**：
  * *`tusd` 独立进程*：Go 编写的官方参考服务端。它能处理协议，但在上传完成后需要通过 Hook 通知 FileLink。这会产生两套上传状态（`tusd` 的文件状态与 FileLink 的 MySQL 状态），且 Hook 在高并发下面临乱序、幂等与重试补偿难题。
  * *嵌入式最小实现*：在已有的 Tudou HTTP 服务中直接解析 Tus 协议头，直接读写 MySQL 状态机。
* **选型结论**：**嵌入式实现**。
  * 保持单进程架构。MySQL 是上传状态的唯一事实来源。没有跨进程分布式一致性问题，上传完成后直接触发本地存储层的发布流程，可靠性极高。

### 决策 3：为什么采用 Tus 的“顺序追加（Offset）”，而不是 Resumable.js 的“并发分片合并”？
* **方案对比**：
  * *并发分片合并 (Resumable.js / S3 Multipart)*：多个分片并行上传。服务端若分开保存分片，最后需要二次读取并合并（双倍磁盘 I/O 放大）；若采用预分配文件进行 `pwrite` 随机写，则需要复杂的一致性 bitmap 来追踪文件空洞。
  * *顺序追加 (Tus Core)*：基于单调递增的 `committed_offset`，服务端只需维护一个单文件指针，数据流按顺序追加（Append）写入一个 `.part` 临时文件。
* **选型结论**：**顺序追加模型**。
  * 避免了任何分片合并的 I/O 开销，并且允许我们在数据流入时进行增量哈希计算。
  * 单文件指针的状态空间极小，在数据库中只需一个字段 `committed_offset`，配合 CAS（Compare-And-Swap）控制，天然具备并发幂等性。同时，内部网络下单线程顺序 I/O 已经足以打满千兆/万兆带宽，无需为“假设的带宽瓶颈”提前引入并发分片的复杂度。

---

## 3. Tus 协议 Handler 的具体实现

FileLink 的 [ApiRouter](file:///home/wxm/FileLink/src/ApiRouter.cpp) 实现了以下核心 Tus 接口：

1. **`handle_tus_options` (协商阶段)**
   * **方法**：`OPTIONS /uploads`
   * **作用**：询问服务器所支持的 Tus 版本、最大上传限制（限制为 10GB）和可用扩展（如 `creation`）。

2. **`handle_tus_create` (创建阶段)**
   * **方法**：`POST /uploads`
   * **作用**：客户端通过 `Upload-Length` 声明文件总大小，服务器在 MySQL `upload_sessions` 表中插入一条状态为 `UPLOADING`、`committed_offset = 0` 的新记录，并返回 `201 Created` 状态码与 `Location: /uploads/<upload_id>` 头。

3. **`handle_tus_head` (进度查询)**
   * **方法**：`HEAD /uploads/<upload_id>`
   * **作用**：断点续传的关键。客户端在断线重连后，向服务器发起 HEAD 请求。服务器返回 `Upload-Offset`（当前数据库内已落盘的字节数），客户端收到后便从该偏移量处切片继续上传。

4. **`handle_tus_patch` (数据传输)**
   * **方法**：`PATCH /uploads/<upload_id>`
   * **作用**：传输二进制流。客户端在 `Upload-Offset` 头中声明本次发送的起始位置。
   * **强一致性控制**：服务器验证请求中的偏移量是否**等于**数据库记录。若不匹配（如客户端重试发送了已收到的块），直接拒绝并返回 `409 Conflict`。匹配则顺序追加写入磁盘临时文件，并利用 SQL 事务的 CAS 更新 `committed_offset`。

5. **`handle_tus_get_session` (状态查询 - 业务扩展)**
   * **方法**：`GET /uploads/<upload_id>`
   * **作用**：用于获取当前会话的详细 JSON 状态（非 Tus 标准，属于 FileLink 业务扩展）。允许前端查询文件校验和落盘进度（例如从 `UPLOADING` 转换到 `FINALIZING` 或 `COMPLETED`）。

---

## 4. 上传完成与 ObjectStore 原子发布流程

当最后一个 `PATCH` 请求使 `committed_offset == total_size` 时，数据传输完成。为了避免阻塞 HTTP 服务器的 EventLoop，FileLink 采用了**异步 Finalizing** 设计：

1. **状态转换**：会话状态在 MySQL 中被标记为 `FINALIZING`，向客户端返回最后一次 `PATCH` 的成功响应。
2. **Hash 校验**：后台工作线程计算该 `.part` 临时文件的完整 BLAKE3 校验和，并与客户端秒传时提供的 `expected_hash` 进行比对。
3. **内容寻址落盘**：如果哈希一致，调用存储层 `ObjectStore::publish()`，通过文件系统的 `link()`（硬链接）将文件原子性地移动到 `storage/objects/<prefix>/<hash>`。
4. **归档完成**：将 MySQL 会话更新为 `COMPLETED`，临时文件被 `unlink` 清理。逻辑文件正式对用户可见。
