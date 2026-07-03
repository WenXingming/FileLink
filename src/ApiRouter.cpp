#include "ApiRouter.h"
#include "StreamUploader.h"

#include "tudou/http/HttpRequest.h"
#include "tudou/http/HttpResponse.h"

#include <chrono>
#include <exception>
#include <utility>

namespace filelink {

ApiRouter::ApiRouter(HttpServer& server, LocalObjectStore store, std::string storageRoot)
    : server_(server),
      store_(std::move(store)),
      storageRoot_(std::move(storageRoot)) {}

void ApiRouter::registerRoutes() {
    // 使用 std::bind 绑定成员函数到路由回调
    server_.add_get_route("/health", [this](const HttpRequest& req, HttpResponse& res) {
        this->handleHealth(req, res);
    });

    server_.add_post_route("/upload", [this](const HttpRequest& req, HttpResponse& res) {
        this->handleUpload(req, res);
    });
}

void ApiRouter::handleHealth(const HttpRequest&, HttpResponse& response) {
    response = HttpResponse::plain_text(200, "OK", R"({"status":"ok"})");
    response.set_header("Content-Type", "application/json");
}

void ApiRouter::handleUpload(const HttpRequest& req, HttpResponse& response) {
    try {
        // 1. 生成唯一的临时文件路径
        uint64_t reqId = ++reqCounter_;
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        std::string tempPath = storageRoot_ + "/tmp_upload_" + std::to_string(now) + "_" + std::to_string(reqId) + ".tmp";
        
        // 2. 利用 StreamUploader 边写临时文件边算哈希
        StreamUploader uploader(tempPath);
        const std::string& body = req.get_body();
        uploader.appendChunk(body.data(), body.size());
        std::string finalHash = uploader.finalize();
        
        // 3. 将验证后的内容提交到对象存储
        auto result = store_.commit(tempPath, finalHash);
        
        // 4. 构建成功响应
        std::string respBody = "{\"status\":\"success\",\"hash\":\"" + finalHash + "\",\"result\":\"" + 
                               (result.status == CommitStatus::Created ? "created" : "reused") + "\"}";
        response = HttpResponse::plain_text(200, "OK", respBody);
        response.set_header("Content-Type", "application/json");
        
    } catch (const std::exception& ex) {
        std::string errorBody = std::string("{\"status\":\"error\",\"message\":\"") + ex.what() + "\"}";
        response = HttpResponse::plain_text(500, "Internal Server Error", errorBody);
        response.set_header("Content-Type", "application/json");
    }
}

} // namespace filelink
