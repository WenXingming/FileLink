# FileLink 架构重构：引入完整 MVC 架构 (Model-View-Controller)

## 1. 现状痛点
在最初的 MVP 阶段，为了快速跑通全链路，我们采用了“单体路由”的写法。
以 `ApiRouter::handleUpload` 为例，它曾承担了以下所有工作：
1. **HTTP 解析**：从 `HttpRequest` 提取 Body、读取 `X-File-Name` 和 `Host` 等 Header。
2. **业务编排**：调用 `StreamUploader` 进行落盘和哈希计算。
3. **数据持久化**：调用 `LocalObjectStore` 进行落盘和原子提交。
4. **响应封装**：根据业务结果，手动拼接 JSON 字符串，生成 `HttpResponse`。

这导致了典型的“胖路由（Fat Router）”或“上帝类”代码异味。而且手动拼接 JSON 存在安全隐患（如未转义双引号导致 JSON 注入）。

## 2. 目标架构 (三层 MVC 分离)
为了保持代码的清爽和企业级工程标准，我们引入了完整的 MVC 架构来解耦。

### 角色划分：
- **ApiRouter (Controller)**：纯粹的“交通枢纽”。只负责剥离 HTTP 入参，调起 Model 执行业务，然后把 Model 的返回结果丢给 View 渲染 HTTP 响应。
- **FileService (Model)**：纯粹的业务领域专家。负责串联上传、哈希、对象存储的完整业务流。它不知道什么是 HTTP，只认纯 C++ 的基础数据类型。
- **ApiResponseView (View)**：负责 API 响应序列化。将业务结构体转换为安全的 JSON 字符串，并设置标准的 HTTP Status 和 Content-Type 头信息，封装基础的 JSON 转义。

## 3. 核心代码示例

重构后，`ApiRouter` 的逻辑如行云流水般清晰：

```cpp
void ApiRouter::handleUpload(const HttpRequest& req, HttpResponse& response) {
    try {
        // 1. Controller: 提取参数
        std::string fileName = req.get_header("X-File-Name");
        std::string host = req.get_header("Host");
        if (host.empty()) {
            host = "127.0.0.1:8080";
        }
        
        // 2. Model: 呼叫业务服务执行逻辑
        const std::string& body = req.get_body();
        UploadResult result = fileService_.processUpload(body, fileName);
        
        // 3. View: 将业务结果交给视图层去渲染 HTTP 响应
        response = ApiResponseView::uploadSuccess(result, host);
        
    } catch (const std::exception& ex) {
        // View: 渲染错误响应
        response = ApiResponseView::error(500, ex.what());
    }
}
```

通过这种设计，`ApiRouter` 代码量断崖式下降，且每一层都有单一职责，整体架构变得极其清晰和优雅。
