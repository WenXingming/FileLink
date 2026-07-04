#pragma once

#include "ObjectService.h"
#include "StaticFileService.h"
#include "tudou/http/HttpServer.h"

#include <atomic>
#include <string>

namespace filelink {

class ApiRouter {
public:
    /**
     * @brief 构造路由控制器
     * @param server 底层 HTTP 服务器实例
     * @param objectService 文件服务实例
     * @param staticFileService 静态资源服务实例
     */
    ApiRouter(HttpServer& server, ObjectService& objectService, StaticFileService& staticFileService);

    /**
     * @brief 注册所有支持的 API 路由
     */
    void register_routes();

private:
    // 各个具体路由的处理函数
    void handle_index(const HttpRequest& req, HttpResponse& response);
    void handle_health(const HttpRequest& req, HttpResponse& response);
    void handle_upload(const HttpRequest& req, HttpResponse& response);
    void handle_download(const HttpRequest& req, HttpResponse& response);
    void handle_static(const HttpRequest& req, HttpResponse& response);

private:
    std::string infer_mime_type(const std::string& ext) const;

    HttpServer& server_;
    ObjectService& objectService_;
    StaticFileService& staticFileService_;
};

} // namespace filelink
