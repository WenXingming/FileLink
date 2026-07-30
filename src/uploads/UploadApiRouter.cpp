#include "uploads/UploadApiRouter.h"
#include "ApiResponseView.h"
#include "auth/AuthRequestParser.h"
#include "auth/AuthService.h"

#include "tudou/http/HttpRequest.h"
#include "tudou/http/HttpResponse.h"

#include <exception>
#include <utility>

#include "database/UploadSession.h"

namespace filelink {

namespace {

const char kUploadsPrefix[] = "/uploads/";
const uint64_t kMaximumUploadSize = 10ULL * 1024 * 1024 * 1024;

void method_not_allowed(HttpResponse& response) {
    response = ApiResponseView::tus_error(405, "Method Not Allowed", "Method Not Allowed");
}

bool upload_id_from_request(const HttpRequest& request, std::string& out_upload_id,
    HttpResponse& response) {
    const std::string path = request.get_path();
    const std::size_t prefix_length = sizeof(kUploadsPrefix) - 1;
    if (path.size() <= prefix_length) {
        response = ApiResponseView::tus_error(400, "Bad Request", "Missing Upload ID");
        return false;
    }

    out_upload_id = path.substr(prefix_length);
    return true;
}

bool unsigned_header(const HttpRequest& request, const std::string& name, uint64_t& out_value,
    HttpResponse& response) {
    const std::string value = request.get_header(name);
    if (value.empty()) {
        response = ApiResponseView::tus_error(400, "Bad Request", "Missing " + name + " Header");
        return false;
    }

    try {
        out_value = std::stoull(value);
        return true;
    }
    catch (const std::exception&) {
        response = ApiResponseView::tus_error(400, "Bad Request", "Invalid " + name);
        return false;
    }
}

} // namespace

UploadApiRouter::UploadApiRouter(HttpServer& server, UploadService& uploadService,
    AuthService& auth_service)
    : server_(server),
    uploadService_(uploadService),
    auth_service_(auth_service) {
}

void UploadApiRouter::register_routes() {
    server_.add_route("OPTIONS", "/uploads", [this](const HttpRequest& req, HttpResponse& res) {
        handle_tus_options(req, res);
        });

    server_.add_prefix_route("/uploads/", [this](const HttpRequest& req, HttpResponse& res) {
        const std::string method = req.get_method();
        if (method == "HEAD") {
            handle_tus_head(req, res);
        }
        else if (method == "PATCH") {
            handle_tus_patch(req, res);
        }
        else if (method == "GET") {
            handle_tus_get_session(req, res);
        }
        else if (method == "DELETE") {
            handle_tus_terminate(req, res);
        }
        else {
            method_not_allowed(res);
        }
        });

    server_.add_post_route("/uploads", [this](const HttpRequest& req, HttpResponse& res) {
        handle_tus_create(req, res);
        });

}

void UploadApiRouter::handle_tus_options(const HttpRequest&, HttpResponse& response) {
    response = ApiResponseView::tus_options();
}

bool UploadApiRouter::authenticate_upload_request(const HttpRequest& req, AuthenticatedUser& out_user,
    HttpResponse& response) {
    std::string session_token;
    if (!AuthRequestParser::parse_session_token(req, session_token)) {
        response = ApiResponseView::tus_error(401, "Unauthorized", "Unauthorized");
        return false;
    }

    const CurrentUserResult result = auth_service_.current_user(session_token, out_user);
    switch (result) {
    case CurrentUserResult::Success:
        return true;
    case CurrentUserResult::InvalidSession:
        response = ApiResponseView::tus_error(401, "Unauthorized", "Unauthorized");
        return false;
    case CurrentUserResult::SystemError:
        response = ApiResponseView::tus_error(500, "Internal Server Error", "Authentication Failed");
        return false;
    }
    return false;
}

void UploadApiRouter::handle_tus_head(const HttpRequest& req, HttpResponse& response) {
    if (req.get_method() != "HEAD") {
        method_not_allowed(response);
        return;
    }

    AuthenticatedUser user;
    if (!authenticate_upload_request(req, user, response)) {
        return;
    }

    std::string upload_id;
    if (!upload_id_from_request(req, upload_id, response)) {
        return;
    }
    uint64_t offset = 0;
    uint64_t totalSize = 0;

    try {
        const bool found = uploadService_.get_session_progress(
            user.user_id, upload_id, offset, totalSize);
        if (!found) {
            response = ApiResponseView::tus_error(404, "Not Found", "Upload Session Not Found");
            return;
        }

        response = ApiResponseView::tus_head(offset, totalSize);
    }
    catch (const std::exception& ex) {
        response = ApiResponseView::tus_error(500, "Internal Server Error", ex.what());
    }
}

void UploadApiRouter::handle_tus_create(const HttpRequest& req, HttpResponse& response) {
    response.set_header("Tus-Resumable", "1.0.0");

    if (req.get_method() != "POST") {
        method_not_allowed(response);
        return;
    }

    AuthenticatedUser user;
    if (!authenticate_upload_request(req, user, response)) {
        return;
    }

    uint64_t totalSize = 0;
    if (!unsigned_header(req, "Upload-Length", totalSize, response)) {
        return;
    }

    if (totalSize > kMaximumUploadSize) {
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
        const bool created = uploadService_.create_session(
            user.user_id, totalSize, metadata, host, uploadIdHex);
        if (!created) {
            response = ApiResponseView::tus_error(500, "Internal Server Error", "Failed to create session");
            return;
        }

        response = ApiResponseView::tus_created(uploadIdHex, host);
    }
    catch (const std::exception& ex) {
        response = ApiResponseView::tus_error(500, "Internal Server Error", ex.what());
    }
}

void UploadApiRouter::handle_tus_patch(const HttpRequest& req, HttpResponse& response) {
    response.set_header("Tus-Resumable", "1.0.0");

    if (req.get_method() != "PATCH") {
        method_not_allowed(response);
        return;
    }

    AuthenticatedUser user;
    if (!authenticate_upload_request(req, user, response)) {
        return;
    }

    std::string upload_id;
    if (!upload_id_from_request(req, upload_id, response)) {
        return;
    }

    const std::string contentType = req.get_header("Content-Type");
    if (contentType != "application/offset+octet-stream") {
        response = ApiResponseView::tus_error(415, "Unsupported Media Type", "Content-Type must be application/offset+octet-stream");
        return;
    }

    uint64_t clientOffset = 0;
    if (!unsigned_header(req, "Upload-Offset", clientOffset, response)) {
        return;
    }

    try {
        uint64_t newOffset = 0;
        const UploadChunkResult result = uploadService_.write_session_chunk(user.user_id, upload_id,
            clientOffset, req.get_body(), newOffset);
        if (result == UploadChunkResult::Success) {
            response = ApiResponseView::tus_patched(newOffset);
        }
        else if (result == UploadChunkResult::OffsetMismatch) {
            response = ApiResponseView::tus_error(409, "Conflict", "Offset Mismatch");
        }
        else if (result == UploadChunkResult::InvalidChunkSize) {
            response = ApiResponseView::tus_error(400, "Bad Request", "Invalid Chunk Size or Range");
        }
        else if (result == UploadChunkResult::SessionNotFound) {
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

void UploadApiRouter::handle_tus_get_session(const HttpRequest& req, HttpResponse& response) {
    if (req.get_method() != "GET") {
        method_not_allowed(response);
        return;
    }

    AuthenticatedUser user;
    if (!authenticate_upload_request(req, user, response)) {
        return;
    }

    std::string upload_id;
    if (!upload_id_from_request(req, upload_id, response)) {
        return;
    }

    try {
        db::UploadSession session;
        const bool found = uploadService_.get_session(user.user_id, upload_id, session);
        if (!found) {
            response = ApiResponseView::tus_error(404, "Not Found", "Upload Session Not Found");
            return;
        }

        response = ApiResponseView::tus_session_status(session);
    }
    catch (const std::exception& ex) {
        response = ApiResponseView::tus_error(500, "Internal Server Error", ex.what());
    }
}

void UploadApiRouter::handle_tus_terminate(const HttpRequest& req, HttpResponse& response) {
    response.set_header("Tus-Resumable", "1.0.0");

    if (req.get_method() != "DELETE") {
        method_not_allowed(response);
        return;
    }

    AuthenticatedUser user;
    if (!authenticate_upload_request(req, user, response)) {
        return;
    }

    std::string upload_id;
    if (!upload_id_from_request(req, upload_id, response)) {
        return;
    }

    try {
        const UploadTerminationResult result = uploadService_.terminate_session(user.user_id, upload_id);
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
