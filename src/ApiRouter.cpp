#include "ApiRouter.h"
#include "ApiResponseView.h"
#include "auth/AuthService.h"

#include "tudou/http/HttpRequest.h"
#include "tudou/http/HttpResponse.h"

#include <chrono>
#include <exception>
#include <utility>

#include "UploadService.h"
#include "db/UploadSession.h"
namespace filelink {

ApiRouter::ApiRouter(HttpServer& server, StaticFileService& staticFileService,
    UploadService& uploadService, RequestAuthenticator& requestAuthenticator)
    : server_(server),
    staticFileService_(staticFileService),
    uploadService_(uploadService),
    requestAuthenticator_(requestAuthenticator) {
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

    server_.add_route("OPTIONS", "/uploads", [this](const HttpRequest& req, HttpResponse& res) {
        this->handle_tus_options(req, res);
        });

    server_.add_prefix_route("/uploads/", [this](const HttpRequest& req, HttpResponse& res) {
        std::string method = req.get_method();
        if (method == "HEAD") {
            this->handle_tus_head(req, res);
        }
        else if (method == "PATCH") {
            this->handle_tus_patch(req, res);
        }
        else if (method == "GET") {
            this->handle_tus_get_session(req, res);
        }
        else if (method == "DELETE") {
            this->handle_tus_terminate(req, res);
        }
        else {
            res = ApiResponseView::tus_error(405, "Method Not Allowed", "Method Not Allowed");
        }
        });

    server_.add_post_route("/uploads", [this](const HttpRequest& req, HttpResponse& res) {
        this->handle_tus_create(req, res);
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

void ApiRouter::handle_tus_options(const HttpRequest&, HttpResponse& response) {
    response = ApiResponseView::tus_options();
}

bool ApiRouter::authenticate_upload_request(const HttpRequest& req, AuthenticatedUser& out_user,
    HttpResponse& response) {
    const RequestAuthResult auth_result = requestAuthenticator_.authenticate(req, out_user);
    if (auth_result == RequestAuthResult::Authenticated) {
        return true;
    }

    response = ApiResponseView::tus_error(auth_result == RequestAuthResult::SystemError ? 500 : 401,
        auth_result == RequestAuthResult::SystemError ? "Internal Server Error" : "Unauthorized",
        auth_result == RequestAuthResult::SystemError ? "Authentication Failed" : "Unauthorized");
    return false;
}

void ApiRouter::handle_tus_head(const HttpRequest& req, HttpResponse& response) {
    if (req.get_method() != "HEAD") {
        response = ApiResponseView::tus_error(405, "Method Not Allowed", "Method Not Allowed");
        return;
    }

    AuthenticatedUser user;
    if (!authenticate_upload_request(req, user, response)) {
        return;
    }

    const std::string prefix = "/uploads/";
    const std::string path = req.get_path();
    if (path.size() <= prefix.size()) {
        response = ApiResponseView::tus_error(400, "Bad Request", "Missing Upload ID");
        return;
    }

    std::string uploadIdRaw = path.substr(prefix.size());
    uint64_t offset = 0;
    uint64_t totalSize = 0;

    try {
        if (!uploadService_.get_session_progress(user.user_id, uploadIdRaw, offset, totalSize)) {
            response = ApiResponseView::tus_error(404, "Not Found", "Upload Session Not Found");
            return;
        }

        response = ApiResponseView::tus_head(offset, totalSize);
    }
    catch (const std::exception& ex) {
        response = ApiResponseView::tus_error(500, "Internal Server Error", ex.what());
    }
}

void ApiRouter::handle_tus_create(const HttpRequest& req, HttpResponse& response) {
    response.set_header("Tus-Resumable", "1.0.0");

    if (req.get_method() != "POST") {
        response = ApiResponseView::tus_error(405, "Method Not Allowed", "Method Not Allowed");
        return;
    }

    AuthenticatedUser user;
    if (!authenticate_upload_request(req, user, response)) {
        return;
    }

    std::string uploadLengthStr = req.get_header("Upload-Length");
    if (uploadLengthStr.empty()) {
        response = ApiResponseView::tus_error(400, "Bad Request", "Missing Upload-Length Header");
        return;
    }

    uint64_t totalSize = 0;
    try {
        totalSize = std::stoull(uploadLengthStr);
    }
    catch (...) {
        response = ApiResponseView::tus_error(400, "Bad Request", "Invalid Upload-Length");
        return;
    }

    if (totalSize > 10737418240ULL) { // 10GB limit
        response = ApiResponseView::tus_error(413, "Payload Too Large", "Upload-Length Exceeds Limit");
        return;
    }

    std::string metadata = req.get_header("Upload-Metadata");
    std::string host = req.get_header("Host");
    if (host.empty()) {
        host = "127.0.0.1:8080";
    }

    try {
        std::string uploadIdHex;
        if (!uploadService_.create_session(user.user_id, totalSize, metadata, host, uploadIdHex)) {
            response = ApiResponseView::tus_error(500, "Internal Server Error", "Failed to create session");
            return;
        }

        response = ApiResponseView::tus_created(uploadIdHex, host);
    }
    catch (const std::exception& ex) {
        response = ApiResponseView::tus_error(500, "Internal Server Error", ex.what());
    }
}

void ApiRouter::handle_tus_patch(const HttpRequest& req, HttpResponse& response) {
    response.set_header("Tus-Resumable", "1.0.0");

    if (req.get_method() != "PATCH") {
        response = ApiResponseView::tus_error(405, "Method Not Allowed", "Method Not Allowed");
        return;
    }

    AuthenticatedUser user;
    if (!authenticate_upload_request(req, user, response)) {
        return;
    }

    const std::string prefix = "/uploads/";
    const std::string path = req.get_path();
    if (path.size() <= prefix.size()) {
        response = ApiResponseView::tus_error(400, "Bad Request", "Missing Upload ID");
        return;
    }
    std::string uploadIdRaw = path.substr(prefix.size());

    std::string contentType = req.get_header("Content-Type");
    if (contentType != "application/offset+octet-stream") {
        response = ApiResponseView::tus_error(415, "Unsupported Media Type", "Content-Type must be application/offset+octet-stream");
        return;
    }

    std::string uploadOffsetStr = req.get_header("Upload-Offset");
    if (uploadOffsetStr.empty()) {
        response = ApiResponseView::tus_error(400, "Bad Request", "Missing Upload-Offset Header");
        return;
    }

    uint64_t clientOffset = 0;
    try {
        clientOffset = std::stoull(uploadOffsetStr);
    }
    catch (...) {
        response = ApiResponseView::tus_error(400, "Bad Request", "Invalid Upload-Offset");
        return;
    }

    try {
        uint64_t newOffset = 0;
        UploadChunkResult rc = uploadService_.write_session_chunk(user.user_id, uploadIdRaw,
            clientOffset, req.get_body(), newOffset);
        if (rc == UploadChunkResult::Success) {
            response = ApiResponseView::tus_patched(newOffset);
        }
        else if (rc == UploadChunkResult::OffsetMismatch) {
            response = ApiResponseView::tus_error(409, "Conflict", "Offset Mismatch");
        }
        else if (rc == UploadChunkResult::InvalidChunkSize) {
            response = ApiResponseView::tus_error(400, "Bad Request", "Invalid Chunk Size or Range");
        }
        else if (rc == UploadChunkResult::SessionNotFound) {
            response = ApiResponseView::tus_error(404, "Not Found", "Upload Session Not Found");
        }
        else {
            response = ApiResponseView::tus_error(500, "Internal Server Error", "Chunk Write Failed");
        }
    }
    catch (const std::exception& ex) {
        response = ApiResponseView::tus_error(500, "Internal Server Error", ex.what());
    }
}

void ApiRouter::handle_tus_get_session(const HttpRequest& req, HttpResponse& response) {
    if (req.get_method() != "GET") {
        response = ApiResponseView::tus_error(405, "Method Not Allowed", "Method Not Allowed");
        return;
    }

    AuthenticatedUser user;
    if (!authenticate_upload_request(req, user, response)) {
        return;
    }

    const std::string prefix = "/uploads/";
    const std::string path = req.get_path();
    if (path.size() <= prefix.size()) {
        response = ApiResponseView::tus_error(400, "Bad Request", "Missing Upload ID");
        return;
    }
    std::string uploadIdRaw = path.substr(prefix.size());

    try {
        db::UploadSession session;
        if (!uploadService_.get_session(user.user_id, uploadIdRaw, session)) {
            response = ApiResponseView::tus_error(404, "Not Found", "Upload Session Not Found");
            return;
        }

        response = ApiResponseView::tus_session_status(session);
    }
    catch (const std::exception& ex) {
        response = ApiResponseView::tus_error(500, "Internal Server Error", ex.what());
    }
}

void ApiRouter::handle_tus_terminate(const HttpRequest& req, HttpResponse& response) {
    response.set_header("Tus-Resumable", "1.0.0");

    if (req.get_method() != "DELETE") {
        response = ApiResponseView::tus_error(405, "Method Not Allowed", "Method Not Allowed");
        return;
    }

    AuthenticatedUser user;
    if (!authenticate_upload_request(req, user, response)) {
        return;
    }

    const std::string prefix = "/uploads/";
    const std::string path = req.get_path();
    if (path.size() <= prefix.size()) {
        response = ApiResponseView::tus_error(400, "Bad Request", "Missing Upload ID");
        return;
    }
    std::string uploadIdRaw = path.substr(prefix.size());

    try {
        const UploadTerminationResult result = uploadService_.terminate_session(user.user_id, uploadIdRaw);
        if (result == UploadTerminationResult::Finalizing) {
            response = ApiResponseView::tus_error(409, "Conflict", "Upload Is Finalizing");
            return;
        }
        if (result == UploadTerminationResult::SessionNotFound) {
            response = ApiResponseView::tus_error(404, "Not Found", "Upload Session Not Found or Cannot Be Terminated");
            return;
        }
        if (result == UploadTerminationResult::SystemError) {
            response = ApiResponseView::tus_error(500, "Internal Server Error", "Failed to Terminate Upload");
            return;
        }

        response.set_status(204, "No Content");
    }
    catch (const std::exception& ex) {
        response = ApiResponseView::tus_error(500, "Internal Server Error", ex.what());
    }
}

} // namespace filelink
