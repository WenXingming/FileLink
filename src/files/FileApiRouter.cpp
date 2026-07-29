// ============================================================================
// Files HTTP Controller 实现：按解析/认证、调用 Service、选择 View 的顺序处理请求。
// 路径格式由 FileRequestParser 负责，JSON 与状态码由 FileResponseView 负责。
// ============================================================================

#include "FileApiRouter.h"

#include "ApiResponseView.h"
#include "FileRequestParser.h"
#include "FileResponseView.h"
#include "FileService.h"
#include "auth/AuthRequestParser.h"
#include "auth/AuthService.h"
#include "shares/ShareApiRouter.h"
#include "storage/ObjectStore.h"
#include "tudou/http/HttpRequest.h"
#include "tudou/http/HttpResponse.h"
#include "tudou/http/HttpServer.h"

#include <exception>
#include <iomanip>
#include <sstream>

namespace filelink {

namespace {

std::string hex_encode(const std::string& bytes) {
    std::stringstream stream;
    stream << std::hex << std::setfill('0');
    for (unsigned char value : bytes) {
        stream << std::setw(2) << static_cast<int>(value);
    }
    return stream.str();
}

bool authenticate_request(AuthService& auth_service, const HttpRequest& request,
    AuthenticatedUser& out_user, HttpResponse& response) {
    std::string session_token;
    if (!AuthRequestParser::parse_session_token(request, session_token)) {
        response = FileResponseView::unauthorized();
        return false;
    }

    switch (auth_service.current_user(session_token, out_user)) {
    case CurrentUserResult::Success:
        return true;
    case CurrentUserResult::InvalidSession:
        response = FileResponseView::unauthorized();
        return false;
    case CurrentUserResult::SystemError:
        response = FileResponseView::server_error("Authentication failed");
        return false;
    }
    return false;
}

} // namespace

FileApiRouter::FileApiRouter(HttpServer& server, FileService& file_service,
    const ObjectStore& object_store, AuthService& auth_service,
    ShareApiRouter& share_api_router)
    : server_(server), file_service_(file_service), object_store_(object_store),
      auth_service_(auth_service), share_api_router_(share_api_router) {}

void FileApiRouter::register_routes() {
    server_.add_get_route("/files", [this](const HttpRequest& request, HttpResponse& response) {
        handle_list_files(request, response);
    });
    server_.add_prefix_route("/files/", [this](const HttpRequest& request, HttpResponse& response) {
        handle_file_subresource(request, response);
    });
}

void FileApiRouter::handle_file_subresource(const HttpRequest& request, HttpResponse& response) {
    if (share_api_router_.handle_management_request(request, response)) {
        return;
    }
    if (request.get_method() == "GET") {
        handle_download(request, response);
        return;
    }
    if (request.get_method() == "DELETE") {
        handle_delete(request, response);
        return;
    }
    response = FileResponseView::not_found();
}

void FileApiRouter::handle_list_files(const HttpRequest& request, HttpResponse& response) {
    AuthenticatedUser user;
    if (!authenticate_request(auth_service_, request, user, response)) {
        return;
    }

    try {
        response = FileResponseView::file_list(file_service_.list_files(user.user_id));
    } catch (const std::exception&) {
        response = FileResponseView::server_error("File listing failed");
    }
}

void FileApiRouter::handle_download(const HttpRequest& request, HttpResponse& response) {
    std::string file_id;
    if (!FileRequestParser::parse_download_file_id(request, file_id)) {
        response = FileResponseView::not_found();
        return;
    }

    AuthenticatedUser user;
    if (!authenticate_request(auth_service_, request, user, response)) {
        return;
    }

    try {
        db::File file;
        if (!file_service_.find_file(user.user_id, file_id, file)) {
            response = FileResponseView::not_found();
            return;
        }

        const std::string object_key = object_store_.get_object_key(hex_encode(file.content_hash));
        response = ApiResponseView::download_redirect(object_key, file.display_name);
    } catch (const std::exception&) {
        response = FileResponseView::server_error("File download failed");
    }
}

void FileApiRouter::handle_delete(const HttpRequest& request, HttpResponse& response) {
    std::string file_id;
    if (!FileRequestParser::parse_file_id(request, file_id)) {
        response = FileResponseView::not_found();
        return;
    }

    AuthenticatedUser user;
    if (!authenticate_request(auth_service_, request, user, response)) {
        return;
    }

    try {
        if (!file_service_.delete_file(user.user_id, file_id)) {
            response = FileResponseView::not_found();
            return;
        }
        response = FileResponseView::deleted();
    } catch (const std::exception&) {
        response = FileResponseView::server_error("File deletion failed");
    }
}

} // namespace filelink
