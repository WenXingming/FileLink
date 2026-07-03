# Tudou 框架流式解析升级计划 (TODO)

## 1. 当前架构瓶颈分析
在 FileLink 的大文件上传设计中，为了避免巨大的内存占用，我们采用了“边接收边落盘计算哈希”的流式处理模型（即 `StreamUploader`）。

然而，当前底层依赖的高性能网络框架 `Tudou` 在 HTTP 解析层（`HttpContext` 基于 `llhttp`）采用的是**全缓冲模型**：
- 它会在 `on_body_impl` 回调中，把网络接收到的所有 HTTP Body 片段无脑拼接并累加到 `HttpRequest::body_` 这个 `std::string` 对象中。
- 只有当整条 HTTP 消息完全接收完毕，触发 `on_message_complete` 时，才会启动路由分发，将装满数据的 `HttpRequest` 交给业务 Handler。

**致命影响**：如果用户上传一个 5GB 的大文件，`Tudou` 将试图在堆内存中分配 5GB 的 `std::string`，这将直接导致服务器 OOM 崩溃。

## 2. MVP 妥协方案（当前执行中）
为了贯彻“小步快跑”的增量开发原则，我们在第一阶段（MVP）采取妥协策略：
- **暂不修改 Tudou 框架**，接受其内存全缓冲的限制。
- 在 `FileLink` 的 `POST /upload` 接口中，取出完整的 `HttpRequest::get_body()`，一次性传给 `StreamUploader::appendChunk`。
- **约束**：此阶段仅能用来测试和验证“小文件”的全链路（HTTP -> 暂存 -> 哈希 -> 落盘对象库）闭环正确性。

## 3. Tudou 框架重构计划 (Refactor TODO)
将 Tudou 打造成支持**“流式路由 (Streaming Route)”**的现代网络框架，是后续的重要技术亮点，极其适合作为简历和面试中的硬核亮点。

### 改造路线图：
1. **暴露头部解析完成拦截点**
   - 接入 `llhttp` 的 `on_headers_complete` 回调。
   - 在此节点，虽然 Body 还没开始传输，但已经可以获取到完整的 URL 和请求头。

2. **路由前置匹配与流式升级**
   - 在 `on_headers_complete` 触发时，立即进入 `Router` 查找。
   - 如果匹配到这是一个注册为流式的接口（例如 `server.add_streaming_post_route("/upload", ...)`），则将 `HttpContext` 标记为流模式，并返回一个流式回调句柄。

3. **零拷贝/流式透传 Body**
   - 在流模式下，后续每次触发 `llhttp` 的 `on_body` 回调时，框架不再往 `std::string body_` 里无脑追加，而是直接调用业务层的流式回调句柄（把指针和长度透传过去）。
   - 业务层（如 FileLink）的流式句柄内部，实时调用 `StreamUploader::appendChunk`，实现大文件的高速落盘与哈希，彻底解放内存限制。

4. **开源贡献与集成**
   - 在本地 `Tudou` 源码完成改造和单元测试。
   - 推送至 GitHub `WenXingming/Tudou` 仓库，并将 `FileLink` 的 `CMakeLists.txt` 更新以拉取最新的 Tudou 框架版本。


```text
我已经将之前对底层框架的分析、妥协方案以及未来的深度重构路线，详细整理并记录在了 TODO_Tudou_Streaming.md
文档中。正如你所说，如果我们在简历里写上**“基于
  llhttp
  将开源全缓冲网络框架改造成高性能流式网络框架，突破大文件内存屏障”**，这绝对是一个极具技术深度的闪光点！这不仅能证明你熟悉业务逻辑，更能证明你有
  能力深入造轮子、解决系统底层瓶颈。
```