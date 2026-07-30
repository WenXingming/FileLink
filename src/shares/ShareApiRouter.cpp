// ============================================================================
// Shares HTTP Controller 实现：按解析/认证、调用 Service、选择 View 的顺序处理请求。
// 路径和 JSON 输入由 ShareRequestParser 负责，响应格式由 ShareResponseView 负责。
// ============================================================================

#include "ShareApiRouter.h"

#include "ShareRequestParser.h"
#include "ShareResponseView.h"
#include "ShareService.h"
#include "auth/AuthRequestParser.h"
#include "auth/AuthService.h"
#include "tudou/http/HttpRequest.h"
#include "tudou/http/HttpResponse.h"
#include "tudou/http/HttpServer.h"

#include <exception>

namespace filelink {

namespace {

bool authenticate_request(AuthService& auth_service, const HttpRequest& request,
    AuthenticatedUser& user, HttpResponse& response) {
    std::string session_token;
    if (!AuthRequestParser::parse_session_token(request, session_token)) {
        response = ShareResponseView::unauthorized();
        return false;
    }

    const CurrentUserResult result = auth_service.current_user(session_token, user);
    switch (result) {
    case CurrentUserResult::Success:
        return true;
    case CurrentUserResult::InvalidSession:
        response = ShareResponseView::unauthorized();
        return false;
    case CurrentUserResult::SystemError:
        response = ShareResponseView::server_error("Authentication failed");
        return false;
    }
    return false;
}

} // namespace

ShareApiRouter::ShareApiRouter(HttpServer& server, ShareService& share_service,
    AuthService& auth_service)
    : server_(server), share_service_(share_service), auth_service_(auth_service) {}

void ShareApiRouter::register_routes() {
    server_.add_prefix_route("/shares/", [this](const HttpRequest& request, HttpResponse& response) {
        const std::string method = request.get_method();

        std::string file_id;
        if (ShareRequestParser::parse_collection_file_id(request, file_id)) {
            if (method == "POST") {
                handle_create(request, response, file_id);
                return;
            }
            if (method == "GET") {
                handle_list(request, response, file_id);
                return;
            }
            response = ShareResponseView::not_found();
            return;
        }

        std::string share_id;
        if (!ShareRequestParser::parse_share_ids(request, file_id, share_id)
            || method != "DELETE") {
            response = ShareResponseView::not_found();
            return;
        }
        handle_revoke(request, response, file_id, share_id);
    });
}

void ShareApiRouter::handle_create(const HttpRequest& request, HttpResponse& response,
    const std::string& file_id) {
    AuthenticatedUser user;
    if (!authenticate_request(auth_service_, request, user, response)) {
        return;
    }

    std::time_t expiry;
    if (!ShareRequestParser::parse_expiry(request, expiry)) {
        response = ShareResponseView::invalid_expiry();
        return;
    }

    CreatedShare share;
    const CreateShareResult result = share_service_.create_share(user.user_id, file_id, expiry, share);
    switch (result) {
    case CreateShareResult::Success:
        response = ShareResponseView::created(share);
        return;
    case CreateShareResult::FileNotFound:
        response = ShareResponseView::not_found();
        return;
    case CreateShareResult::InvalidExpiry:
        response = ShareResponseView::invalid_expiry();
        return;
    case CreateShareResult::SystemError:
        response = ShareResponseView::server_error("Share creation failed");
        return;
    }
}

void ShareApiRouter::handle_list(const HttpRequest& request, HttpResponse& response,
    const std::string& file_id) {
    AuthenticatedUser user;
    if (!authenticate_request(auth_service_, request, user, response)) {
        return;
    }

    try {
        std::vector<db::Share> shares;
        const bool found = share_service_.list_shares(user.user_id, file_id, shares);
        if (!found) {
            response = ShareResponseView::not_found();
            return;
        }
        response = ShareResponseView::share_list(shares);
    } catch (const std::exception&) {
        response = ShareResponseView::server_error("Share listing failed");
    }
}

void ShareApiRouter::handle_revoke(const HttpRequest& request, HttpResponse& response,
    const std::string& file_id, const std::string& share_id) {
    AuthenticatedUser user;
    if (!authenticate_request(auth_service_, request, user, response)) {
        return;
    }

    const RevokeShareResult result = share_service_.revoke_share(user.user_id, file_id, share_id);
    switch (result) {
    case RevokeShareResult::Success:
        response = ShareResponseView::revoked();
        return;
    case RevokeShareResult::FileNotFound:
    case RevokeShareResult::ShareNotFound:
        response = ShareResponseView::not_found();
        return;
    case RevokeShareResult::SystemError:
        response = ShareResponseView::server_error("Share revocation failed");
        return;
    }
}

} // namespace filelink
