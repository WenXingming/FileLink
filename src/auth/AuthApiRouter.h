#pragma once

#include "tudou/http/HttpServer.h"
#include "RequestAuthenticator.h"

namespace filelink {

// =======================================================================
// AuthApiRouter：注册认证相关 HTTP 路由，并将请求转交给 AuthService。
// =======================================================================
class AuthApiRouter {
    friend class AuthApiTest;

public:
    AuthApiRouter(HttpServer& server, AuthService& auth_service,
        RequestAuthenticator& request_authenticator)
        : server_(server), auth_service_(auth_service), request_authenticator_(request_authenticator) {}

    void register_routes();

private:
    void handle_register(const HttpRequest& request, HttpResponse& response);
    void handle_login(const HttpRequest& request, HttpResponse& response);
    void handle_current_user(const HttpRequest& request, HttpResponse& response);
    void handle_logout(const HttpRequest& request, HttpResponse& response);

    HttpServer& server_;
    AuthService& auth_service_;
    RequestAuthenticator& request_authenticator_;
};

} // namespace filelink
