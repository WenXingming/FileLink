// ============================================================================
// Files HTTP Controller：注册私有文件列表和删除接口。
// 只编排请求解析、认证、FileService 和响应 View。
// ============================================================================

#pragma once

class HttpRequest;
class HttpResponse;
class HttpServer;

namespace filelink {

class AuthService;
class FileService;

class FileApiRouter {
public:
    FileApiRouter(HttpServer& server, FileService& file_service, AuthService& auth_service);

    void register_routes();

private:
    void handle_list_files(const HttpRequest& request, HttpResponse& response);
    void handle_delete_file(const HttpRequest& request, HttpResponse& response);

private:
    HttpServer& server_;
    FileService& file_service_;
    AuthService& auth_service_;
};

} // namespace filelink
