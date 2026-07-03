#include "ApiRouter.h"
#include "StreamUploader.h"

#include "tudou/http/HttpRequest.h"
#include "tudou/http/HttpResponse.h"

#include <chrono>
#include <exception>
#include <fstream>
#include <sys/stat.h>
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

    server_.add_prefix_route("/objects/", [this](const HttpRequest& req, HttpResponse& res) {
        this->handleDownload(req, res);
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

void ApiRouter::handleDownload(const HttpRequest& req, HttpResponse& response) {
    if (req.get_method() != "GET") {
        response = HttpResponse::plain_text(405, "Method Not Allowed", "Method Not Allowed\n");
        return;
    }

    // 提取 hash: req.get_path() 会类似于 /objects/d74981efa70a0c880b8d8c1985d075dbcbf679b99a5f9914e5aaf96b831a9e24
    const std::string prefix = "/objects/";
    const std::string path = req.get_path();
    if (path.size() <= prefix.size()) {
        response = HttpResponse::plain_text(400, "Bad Request", "Missing Hash\n");
        return;
    }

    std::string hash = path.substr(prefix.size());
    try {
        std::string objectPath = store_.getObjectPath(hash);
        
        // 检查文件是否存在
        struct stat info;
        if (::stat(objectPath.c_str(), &info) != 0) {
            response = HttpResponse::plain_text(404, "Not Found", "Object Not Found\n");
            return;
        }

        // 妥协版 MVP 读取策略：一次性读入内存
        // TODO: 升级 Tudou 框架支持 sendfile 或流式写入 HTTP 响应
        std::ifstream ifs(objectPath, std::ios::binary);
        if (!ifs) {
            response = HttpResponse::plain_text(500, "Internal Server Error", "Failed to open object\n");
            return;
        }
        
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        response.set_status(200, "OK");
        response.set_body(content);
        response.set_header("Content-Type", "application/octet-stream");
        response.set_header("Content-Length", std::to_string(content.size()));
    } catch (const std::exception& ex) {
        response = HttpResponse::plain_text(400, "Bad Request", std::string(ex.what()) + "\n");
    }
}

} // namespace filelink
