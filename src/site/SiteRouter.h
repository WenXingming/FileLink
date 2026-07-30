// ============================================================================
// Site HTTP Controller：注册主页、健康检查和静态资源路由。
// 只编排请求、静态文件读取和响应 View，不处理文件格式或磁盘细节。
// ============================================================================

#pragma once

class HttpRequest;
class HttpResponse;
class HttpServer;

namespace filelink {

class StaticFileService;

class SiteRouter {
public:
    SiteRouter(HttpServer& server, StaticFileService& static_file_service);

    void register_routes();

private:
    void handle_index(const HttpRequest& request, HttpResponse& response);
    void handle_static(const HttpRequest& request, HttpResponse& response);

private:
    HttpServer& server_;
    StaticFileService& static_file_service_;
};

} // namespace filelink
