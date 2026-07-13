#pragma once

#include "tudou/http/HttpServer.h"

namespace filelink {

class AuthService;

class AuthApiRouter {
    friend class AuthApiTest;

public:
    AuthApiRouter(HttpServer& server, AuthService& auth_service)
        : server_(server), auth_service_(auth_service) {}

    void register_routes();

private:
    void handle_register(const HttpRequest& request, HttpResponse& response);
    void handle_login(const HttpRequest& request, HttpResponse& response);
    void handle_current_user(const HttpRequest& request, HttpResponse& response);
    void handle_logout(const HttpRequest& request, HttpResponse& response);

    HttpServer& server_;
    AuthService& auth_service_;
};

} // namespace filelink
