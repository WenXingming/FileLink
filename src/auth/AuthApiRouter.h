// ============================================================================
// Controller（MVC）：认证 API Controller：注册端点，协调请求解析、AuthService 和响应 View。
// 只编排 HTTP 流程，不实现输入格式、响应格式或认证业务细节。
// ============================================================================

#pragma once

class HttpRequest;
class HttpResponse;
class HttpServer;

namespace filelink {

class AuthService;

class AuthApiRouter {
public:
    AuthApiRouter(HttpServer& server, AuthService& auth_service);

    void register_routes();

    // Tudou 路由回调；测试也通过这些入口验证 HTTP 映射。
    void handle_register(const HttpRequest& request, HttpResponse& response);
    void handle_login(const HttpRequest& request, HttpResponse& response);
    void handle_current_user(const HttpRequest& request, HttpResponse& response);
    void handle_logout(const HttpRequest& request, HttpResponse& response);

private:
    HttpServer& server_;
    AuthService& auth_service_;
};

} // namespace filelink
