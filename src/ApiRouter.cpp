#include "ApiRouter.h"
#include "ApiResponseView.h"

#include "tudou/http/HttpRequest.h"
#include "tudou/http/HttpResponse.h"

#include <chrono>
#include <exception>
#include <fstream>
#include <sys/stat.h>
#include <utility>

namespace filelink {

ApiRouter::ApiRouter(HttpServer& server, ObjectService& objectService, StaticFileService& staticFileService)
    : server_(server),
    objectService_(objectService),
    staticFileService_(staticFileService) {
}

void ApiRouter::register_routes() {
    server_.add_get_route("/", [this](const HttpRequest& req, HttpResponse& res) {
        this->handle_index(req, res);
        });

    server_.add_get_route("/index.html", [this](const HttpRequest& req, HttpResponse& res) {
        this->handle_index(req, res);
        });

    server_.add_get_route("/health", [this](const HttpRequest& req, HttpResponse& res) {
        this->handle_health(req, res);
        });

    server_.add_post_route("/upload", [this](const HttpRequest& req, HttpResponse& res) {
        this->handle_upload(req, res);
        });

    server_.add_prefix_route("/objects/", [this](const HttpRequest& req, HttpResponse& res) {
        this->handle_download(req, res);
        });

    server_.add_prefix_route("/static/", [this](const HttpRequest& req, HttpResponse& res) {
        this->handle_static(req, res);
        });
}

void ApiRouter::handle_index(const HttpRequest&, HttpResponse& response) {
    try {
        // 1. Controller: 设定默认主页 URI
        std::string path = "/index.html";

        // 2. Model: 调 Model 拿数据
        std::string content = staticFileService_.get_asset_content(path);

        // 3. View: 调 View 渲染
        response = ApiResponseView::file(content, ".html");
    }
    catch (const std::exception& ex) {
        // View: 渲染错误响应
        response = ApiResponseView::error(404, "index.html not found");
    }
}

void ApiRouter::handle_health(const HttpRequest&, HttpResponse& response) {
    response = ApiResponseView::health_check();
}

void ApiRouter::handle_upload(const HttpRequest& req, HttpResponse& response) {
    try {
        // 1. Controller: 提取参数
        std::string fileName = req.get_header("X-File-Name");
        std::string host = req.get_header("Host");
        if (host.empty()) {
            host = "127.0.0.1:8080";
        }

        // 2. Model: 呼叫业务服务执行逻辑
        const std::string& body = req.get_body();
        UploadResult result = objectService_.process_upload(body, fileName);

        // 3. View: 将业务结果交给视图层去渲染 HTTP 响应
        response = ApiResponseView::upload_success(result, host);

    }
    catch (const std::exception& ex) {
        // View: 渲染错误响应
        response = ApiResponseView::error(500, ex.what());
    }
}

void ApiRouter::handle_download(const HttpRequest& req, HttpResponse& response) {
    if (req.get_method() != "GET") {
        response = ApiResponseView::error(405, "Method Not Allowed");
        return;
    }

    // 提取 hash: req.get_path() 会类似于 /objects/d74981efa70a0c880b8d8c1985d075dbcbf679b99a5f9914e5aaf96b831a9e24
    const std::string prefix = "/objects/";
    const std::string path = req.get_path();
    if (path.size() <= prefix.size()) {
        response = ApiResponseView::error(400, "Missing Hash");
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
        // 1. Controller: 提取参数已完成 (hash, ext)

        // 2. Model: 调 ObjectService 拿业务对象数据
        std::string content = objectService_.get_object_content(hash);

        // 3. View: 调 View 渲染二进制文件响应
        response = ApiResponseView::file(content, ext);
    }
    catch (const std::invalid_argument& ex) {
        response = ApiResponseView::error(404, ex.what());
    }
    catch (const std::exception& ex) {
        response = ApiResponseView::error(500, ex.what());
    }
}

void ApiRouter::handle_static(const HttpRequest& req, HttpResponse& response) {
    try {
        // 1. Controller: 提取 URI
        std::string path = req.get_path();

        // 2. Model: 调 Model (StaticFileService) 拿数据
        std::string content = staticFileService_.get_asset_content(path);

        // 3. View: 调 View 渲染
        std::string ext = "";
        size_t dotPos = path.find_last_of('.');
        if (dotPos != std::string::npos) {
            ext = path.substr(dotPos);
            for (char& c : ext) c = std::tolower(c);
        }
        response = ApiResponseView::file(content, ext);
    }
    catch (const std::invalid_argument& ex) {
        response = ApiResponseView::error(403, ex.what());
    }
    catch (const std::exception& ex) {
        response = ApiResponseView::error(404, ex.what());
    }
}

} // namespace filelink
