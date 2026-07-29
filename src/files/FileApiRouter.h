// ============================================================================
// Files HTTP Controller：注册私有文件列表、下载和删除接口。
// 只编排请求解析、认证、FileService 和响应 View。
// ============================================================================

#pragma once

class HttpRequest;
class HttpResponse;
class HttpServer;

namespace filelink {

class AuthService;
class FileService;
class ObjectStore;
class ShareApiRouter;

class FileApiRouter {
public:
    FileApiRouter(HttpServer& server, FileService& file_service, const ObjectStore& object_store,
        AuthService& auth_service, ShareApiRouter& share_api_router);

    void register_routes();

    // Tudou 路由回调；测试也通过这些入口验证 HTTP 映射。
    void handle_list_files(const HttpRequest& request, HttpResponse& response);
    void handle_download(const HttpRequest& request, HttpResponse& response);
    void handle_delete(const HttpRequest& request, HttpResponse& response);

private:
    void handle_file_subresource(const HttpRequest& request, HttpResponse& response);

    HttpServer& server_;
    FileService& file_service_;
    const ObjectStore& object_store_;
    AuthService& auth_service_;
    ShareApiRouter& share_api_router_;
};

} // namespace filelink
