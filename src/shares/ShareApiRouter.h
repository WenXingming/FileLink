// ============================================================================
// Shares HTTP Controller：注册分享创建、列表和撤销接口。
// 只编排请求解析、认证、ShareService 和响应 View。
// ============================================================================

#pragma once

#include <string>

class HttpRequest;
class HttpResponse;
class HttpServer;

namespace filelink {

class AuthService;
class ShareService;

class ShareApiRouter {
public:
    ShareApiRouter(HttpServer& server, ShareService& share_service, AuthService& auth_service);

    void register_routes();

private:
    void handle_create(const HttpRequest& request, HttpResponse& response, const std::string& file_id);
    void handle_list(const HttpRequest& request, HttpResponse& response, const std::string& file_id);
    void handle_revoke(const HttpRequest& request, HttpResponse& response, const std::string& file_id, const std::string& share_id);

private:
    HttpServer& server_;
    ShareService& share_service_;
    AuthService& auth_service_;
};

} // namespace filelink
