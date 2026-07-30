// ============================================================================
// Downloads HTTP Controller 实现：区分私有文件 ID 与公开分享 Token 两条下载流程。
// 两条流程最终都交给 DownloadResponseView 构造 Nginx 内部重定向。
// ============================================================================

#include "DownloadApiRouter.h"

#include "DownloadRequestParser.h"
#include "DownloadResponseView.h"
#include "DownloadService.h"
#include "auth/AuthRequestParser.h"
#include "auth/AuthService.h"
#include "tudou/http/HttpRequest.h"
#include "tudou/http/HttpResponse.h"
#include "tudou/http/HttpServer.h"

#include <exception>
#include <string>

namespace filelink {

namespace {

bool authenticate_request(AuthService& auth_service, const HttpRequest& request,
    AuthenticatedUser& user, HttpResponse& response) {
    std::string session_token;
    if (!AuthRequestParser::parse_session_token(request, session_token)) {
        response = DownloadResponseView::unauthorized();
        return false;
    }

    const CurrentUserResult result = auth_service.current_user(session_token, user);
    switch (result) {
    case CurrentUserResult::Success:
        return true;
    case CurrentUserResult::InvalidSession:
        response = DownloadResponseView::unauthorized();
        return false;
    case CurrentUserResult::SystemError:
        response = DownloadResponseView::server_error("Authentication failed");
        return false;
    }
    return false;
}

} // namespace

DownloadApiRouter::DownloadApiRouter(HttpServer& server, DownloadService& download_service,
    AuthService& auth_service)
    : server_(server), download_service_(download_service), auth_service_(auth_service) {}

void DownloadApiRouter::register_routes() {
    // Tudou 按注册顺序匹配前缀，两个具体入口必须放在兜底路由之前。
    server_.add_prefix_route("/downloads/private/", [this](const HttpRequest& request,
        HttpResponse& response) {
        if (request.get_method() != "GET") {
            response = DownloadResponseView::not_found();
            return;
        }
        handle_private_download(request, response);
    });
    server_.add_prefix_route("/downloads/shared/", [this](const HttpRequest& request,
        HttpResponse& response) {
        if (request.get_method() != "GET") {
            response = DownloadResponseView::not_found();
            return;
        }
        handle_shared_download(request, response);
    });
    server_.add_prefix_route("/downloads/", [](const HttpRequest&, HttpResponse& response) {
        response = DownloadResponseView::not_found();
    });
}

void DownloadApiRouter::handle_private_download(const HttpRequest& request,
    HttpResponse& response) {
    std::string file_id;
    if (!DownloadRequestParser::parse_private_file_id(request, file_id)) {
        response = DownloadResponseView::not_found();
        return;
    }

    AuthenticatedUser user;
    if (!authenticate_request(auth_service_, request, user, response)) {
        return;
    }

    try {
        DownloadTarget target;
        const bool found = download_service_.find_private_download(user.user_id, file_id, target);
        if (!found) {
            response = DownloadResponseView::not_found();
            return;
        }
        response = DownloadResponseView::file(target);
    } catch (const std::exception&) {
        response = DownloadResponseView::server_error("File download failed");
    }
}

void DownloadApiRouter::handle_shared_download(const HttpRequest& request,
    HttpResponse& response) {
    std::string token;
    if (!DownloadRequestParser::parse_shared_token(request, token)) {
        response = DownloadResponseView::not_found();
        return;
    }

    try {
        DownloadTarget target;
        const bool found = download_service_.find_shared_download(token, target);
        if (!found) {
            response = DownloadResponseView::not_found();
            return;
        }
        response = DownloadResponseView::file(target);
    } catch (const std::exception&) {
        response = DownloadResponseView::server_error("Share download failed");
    }
}

} // namespace filelink
