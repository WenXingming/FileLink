// ============================================================================
// Site HTTP Controller 实现：把每个请求平铺为读取输入、调用服务、选择响应。
// HTTP 响应格式由 SiteResponseView 维护，磁盘访问由 StaticFileService 维护。
// ============================================================================

#include "SiteRouter.h"

#include "SiteResponseView.h"
#include "StaticFileService.h"
#include "tudou/http/HttpRequest.h"
#include "tudou/http/HttpResponse.h"
#include "tudou/http/HttpServer.h"

#include <exception>
#include <stdexcept>

namespace filelink {

SiteRouter::SiteRouter(HttpServer& server, StaticFileService& static_file_service)
    : server_(server), static_file_service_(static_file_service) {}

void SiteRouter::register_routes() {
    server_.add_get_route("/", [this](const HttpRequest& request, HttpResponse& response) {
        handle_index(request, response);
    });
    server_.add_get_route("/index.html", [this](const HttpRequest& request, HttpResponse& response) {
        handle_index(request, response);
    });
    server_.add_get_route("/health", [](const HttpRequest&, HttpResponse& response) {
        response = SiteResponseView::health_check();
    });
    server_.add_prefix_route("/static/", [this](const HttpRequest& request, HttpResponse& response) {
        handle_static(request, response);
    });
}

void SiteRouter::handle_index(const HttpRequest&, HttpResponse& response) {
    try {
        const std::string path = "/index.html";
        const std::string content = static_file_service_.read_asset(path);
        response = SiteResponseView::asset(path, content);
    } catch (const std::exception&) {
        response = SiteResponseView::not_found("index.html not found");
    }
}

void SiteRouter::handle_static(const HttpRequest& request, HttpResponse& response) {
    try {
        const std::string path = request.get_path();
        const std::string content = static_file_service_.read_asset(path);
        response = SiteResponseView::asset(path, content);
    } catch (const std::invalid_argument& error) {
        response = SiteResponseView::forbidden(error.what());
    } catch (const std::exception& error) {
        response = SiteResponseView::not_found(error.what());
    }
}

} // namespace filelink
