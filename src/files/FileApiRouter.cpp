// ============================================================================
// Files HTTP Controller 实现：按解析/认证、调用 Service、选择 View 的顺序处理请求。
// 路径格式由 FileRequestParser 负责，JSON 与状态码由 FileResponseView 负责。
// ============================================================================

#include "FileApiRouter.h"

#include "FileRequestParser.h"
#include "FileResponseView.h"
#include "FileService.h"
#include "auth/AuthRequestParser.h"
#include "auth/AuthService.h"
#include "tudou/http/HttpRequest.h"
#include "tudou/http/HttpResponse.h"
#include "tudou/http/HttpServer.h"

#include <exception>

namespace filelink {

namespace {

bool authenticate_request(AuthService& auth_service, const HttpRequest& request,
    AuthenticatedUser& out_user, HttpResponse& response) {
    std::string session_token;
    if (!AuthRequestParser::parse_session_token(request, session_token)) {
        response = FileResponseView::unauthorized();
        return false;
    }

    const CurrentUserResult result = auth_service.current_user(session_token, out_user);
    switch (result) {
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
    AuthService& auth_service)
    : server_(server), file_service_(file_service), auth_service_(auth_service) {}

void FileApiRouter::register_routes() {
    server_.add_get_route("/files", [this](const HttpRequest& request, HttpResponse& response) {
        handle_list_files(request, response);
    });
    server_.add_prefix_route("/files/", [this](const HttpRequest& request, HttpResponse& response) {
        if (request.get_method() != "DELETE") {
            response = FileResponseView::not_found();
            return;
        }
        handle_delete_file(request, response);
    });
}

void FileApiRouter::handle_list_files(const HttpRequest& request, HttpResponse& response) {
    AuthenticatedUser user;
    if (!authenticate_request(auth_service_, request, user, response)) {
        return;
    }

    try {
        const std::vector<db::File> files = file_service_.list_files(user.user_id);
        response = FileResponseView::file_list(files);
    } catch (const std::exception&) {
        response = FileResponseView::server_error("File listing failed");
    }
}

void FileApiRouter::handle_delete_file(const HttpRequest& request, HttpResponse& response) {
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
        const bool deleted = file_service_.delete_file(user.user_id, file_id);
        if (!deleted) {
            response = FileResponseView::not_found();
            return;
        }
        response = FileResponseView::deleted();
    } catch (const std::exception&) {
        response = FileResponseView::server_error("File deletion failed");
    }
}

} // namespace filelink
