#pragma once

#include "StaticFileService.h"
#include "tudou/http/HttpServer.h"

namespace filelink {

// =====================================================================
// SiteRouter：提供主页、健康检查和 web 静态资源 HTTP 接口。
// =====================================================================
class SiteRouter {
public:
    SiteRouter(HttpServer& server, StaticFileService& static_file_service)
        : server_(server), static_file_service_(static_file_service) {}

    void register_routes();

private:
    void handle_health(const HttpRequest& request, HttpResponse& response);
    void handle_index(const HttpRequest& request, HttpResponse& response);
    void handle_static(const HttpRequest& request, HttpResponse& response);

    HttpServer& server_;
    StaticFileService& static_file_service_;
};

} // namespace filelink
