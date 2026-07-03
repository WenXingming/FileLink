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

ApiRouter::ApiRouter(HttpServer& server, LocalObjectStore store, std::string storageRoot, std::string webRoot)
    : server_(server),
      store_(std::move(store)),
      storageRoot_(std::move(storageRoot)),
      webRoot_(std::move(webRoot)) {}

void ApiRouter::registerRoutes() {
    server_.add_get_route("/", [this](const HttpRequest& req, HttpResponse& res) {
        this->handleIndex(req, res);
    });

    server_.add_get_route("/index.html", [this](const HttpRequest& req, HttpResponse& res) {
        this->handleIndex(req, res);
    });

    server_.add_get_route("/health", [this](const HttpRequest& req, HttpResponse& res) {
        this->handleHealth(req, res);
    });

    server_.add_post_route("/upload", [this](const HttpRequest& req, HttpResponse& res) {
        this->handleUpload(req, res);
    });

    server_.add_prefix_route("/objects/", [this](const HttpRequest& req, HttpResponse& res) {
        this->handleDownload(req, res);
    });

    server_.add_prefix_route("/static/", [this](const HttpRequest& req, HttpResponse& res) {
        this->handleStatic(req, res);
    });
}

void ApiRouter::handleIndex(const HttpRequest&, HttpResponse& response) {
    std::string indexPath = webRoot_ + "/index.html";
    std::ifstream ifs(indexPath, std::ios::binary);
    if (!ifs) {
        response = HttpResponse::plain_text(404, "Not Found", "<h1>404 Not Found</h1><p>index.html not found</p>");
        response.set_header("Content-Type", "text/html; charset=utf-8");
        return;
    }
    
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    response.set_status(200, "OK");
    response.set_body(content);
    response.set_header("Content-Type", "text/html; charset=utf-8");
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
        
        // 4. 获取文件名并提取后缀
        std::string fileName = req.get_header("X-File-Name");
        std::string ext = "";
        size_t dotPos = fileName.find_last_of('.');
        if (dotPos != std::string::npos) {
            // URL 编码的 . 依然是 .，所以可以直接截取
            ext = fileName.substr(dotPos);
        }

        // 5. 构建前端所需的 url 与成功响应
        std::string host = req.get_header("Host");
        if (host.empty()) {
            host = "127.0.0.1:8080";
        }
        std::string shareUrl = "http://" + host + "/objects/" + finalHash + ext;
        std::string resultStr = (result.status == CommitStatus::Created ? "created" : "reused");
        
        std::string respBody = "{\"status\":\"success\",\"hash\":\"" + finalHash + "\",\"result\":\"" + resultStr + "\",\"url\":\"" + shareUrl + "\"}";
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

    std::string hashWithExt = path.substr(prefix.size());
    std::string hash = hashWithExt;
    std::string ext = "";
    
    size_t dotPos = hashWithExt.find_last_of('.');
    if (dotPos != std::string::npos) {
        hash = hashWithExt.substr(0, dotPos);
        ext = hashWithExt.substr(dotPos);
        // 转小写处理
        for (char& c : ext) c = std::tolower(c);
    }

    try {
        std::string objectPath = store_.getObjectPath(hash);
        
        // 检查文件是否存在
        struct stat info;
        if (::stat(objectPath.c_str(), &info) != 0) {
            response = HttpResponse::plain_text(404, "Not Found", "Object Not Found\n");
            return;
        }

        // 妥协版 MVP 读取策略：一次性读入内存
        std::ifstream ifs(objectPath, std::ios::binary);
        if (!ifs) {
            response = HttpResponse::plain_text(500, "Internal Server Error", "Failed to open object\n");
            return;
        }
        
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        response.set_status(200, "OK");
        response.set_body(content);
        
        // 简单的 MIME 推断
        response.set_header("Content-Type", inferMimeType(ext));
        response.set_header("Content-Length", std::to_string(content.size()));
    } catch (const std::exception& ex) {
        response = HttpResponse::plain_text(400, "Bad Request", std::string(ex.what()) + "\n");
    }
}

std::string ApiRouter::inferMimeType(const std::string& ext) const {
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".png") return "image/png";
    if (ext == ".gif") return "image/gif";
    if (ext == ".txt") return "text/plain; charset=utf-8";
    if (ext == ".html") return "text/html; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".js") return "application/javascript; charset=utf-8";
    if (ext == ".json") return "application/json";
    if (ext == ".pdf") return "application/pdf";
    if (ext == ".mp4") return "video/mp4";
    if (ext == ".svg") return "image/svg+xml";
    return "application/octet-stream";
}

void ApiRouter::handleStatic(const HttpRequest& req, HttpResponse& response) {
    std::string path = req.get_path();
    
    // 安全检查：防路径穿越
    if (path.find("..") != std::string::npos) {
        response = HttpResponse::plain_text(403, "Forbidden", "Forbidden\n");
        return;
    }

    std::string filePath = webRoot_ + path;
    
    struct stat info;
    if (::stat(filePath.c_str(), &info) != 0 || !S_ISREG(info.st_mode)) {
        response = HttpResponse::plain_text(404, "Not Found", "Not Found\n");
        return;
    }

    std::string ext = "";
    size_t dotPos = filePath.find_last_of('.');
    if (dotPos != std::string::npos) {
        ext = filePath.substr(dotPos);
        for (char& c : ext) c = std::tolower(c);
    }
    
    std::ifstream ifs(filePath, std::ios::binary);
    if (!ifs) {
        response = HttpResponse::plain_text(500, "Internal Server Error", "Failed to open file\n");
        return;
    }
    
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    response.set_status(200, "OK");
    response.set_body(content);
    response.set_header("Content-Type", inferMimeType(ext));
}

} // namespace filelink
