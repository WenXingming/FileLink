// ============================================================================
// 认证 API Controller 实现：按解析请求、调用 Service、选择 View 的顺序处理请求。
// 输入格式由 AuthRequestParser 负责，输出格式由 AuthResponseView 负责。
// ============================================================================

#include "AuthApiRouter.h"

#include "AuthRequestParser.h"
#include "AuthResponseView.h"
#include "AuthService.h"
#include "tudou/http/HttpRequest.h"
#include "tudou/http/HttpResponse.h"
#include "tudou/http/HttpServer.h"

namespace filelink {

AuthApiRouter::AuthApiRouter(HttpServer& server, AuthService& auth_service) : server_(server), auth_service_(auth_service) {}

void AuthApiRouter::register_routes() {
    server_.add_post_route("/auth/register", [this](const HttpRequest& request, HttpResponse& response) {
        handle_register(request, response);
    });
    server_.add_post_route("/auth/login", [this](const HttpRequest& request, HttpResponse& response) {
        handle_login(request, response);
    });
    server_.add_get_route("/auth/me", [this](const HttpRequest& request, HttpResponse& response) {
        handle_current_user(request, response);
    });
    server_.add_post_route("/auth/logout", [this](const HttpRequest& request, HttpResponse& response) {
        handle_logout(request, response);
    });
}

void AuthApiRouter::handle_register(const HttpRequest& request, HttpResponse& response) {
    std::string username;
    std::string password;
    if (!AuthRequestParser::parse_credentials(request, username, password)) {
        response = AuthResponseView::error(400, "Bad Request", "Invalid request body");
        return;
    }

    AuthenticatedSession session;
    switch (auth_service_.register_user(username, password, session)) {
    case RegisterResult::Success:
        response = AuthResponseView::registered(session);
        return;
    case RegisterResult::InvalidUsername:
        response = AuthResponseView::error(400, "Bad Request", "Invalid username");
        return;
    case RegisterResult::InvalidPassword:
        response = AuthResponseView::error(400, "Bad Request", "Invalid password");
        return;
    case RegisterResult::UsernameTaken:
        response = AuthResponseView::error(409, "Conflict", "Username already exists");
        return;
    case RegisterResult::SystemError:
        response = AuthResponseView::error(500, "Internal Server Error", "Registration failed");
        return;
    }
}

void AuthApiRouter::handle_login(const HttpRequest& request, HttpResponse& response) {
    std::string username;
    std::string password;
    if (!AuthRequestParser::parse_credentials(request, username, password)) {
        response = AuthResponseView::error(400, "Bad Request", "Invalid request body");
        return;
    }

    AuthenticatedSession session;
    switch (auth_service_.login_user(username, password, session)) {
    case LoginResult::Success:
        response = AuthResponseView::logged_in(session);
        return;
    case LoginResult::InvalidCredentials:
        response = AuthResponseView::error(401, "Unauthorized", "Invalid credentials");
        return;
    case LoginResult::SystemError:
        response = AuthResponseView::error(500, "Internal Server Error", "Login failed");
        return;
    }
}

void AuthApiRouter::handle_current_user(const HttpRequest& request, HttpResponse& response) {
    std::string session_token;
    if (!AuthRequestParser::parse_session_token(request, session_token)) {
        response = AuthResponseView::error(401, "Unauthorized", "Unauthorized");
        return;
    }

    AuthenticatedUser user;
    switch (auth_service_.current_user(session_token, user)) {
    case CurrentUserResult::Success:
        response = AuthResponseView::current_user(user);
        return;
    case CurrentUserResult::InvalidSession:
        response = AuthResponseView::error(401, "Unauthorized", "Unauthorized");
        return;
    case CurrentUserResult::SystemError:
        response = AuthResponseView::error(500, "Internal Server Error", "Current user lookup failed");
        return;
    }
}

void AuthApiRouter::handle_logout(const HttpRequest& request, HttpResponse& response) {
    std::string session_token;
    if (!AuthRequestParser::parse_session_token(request, session_token)) {
        response = AuthResponseView::logged_out();
        return;
    }

    if (!auth_service_.logout(session_token)) {
        response = AuthResponseView::error(500, "Internal Server Error", "Logout failed");
        return;
    }

    response = AuthResponseView::logged_out();
}

} // namespace filelink
