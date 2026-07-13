#pragma once

#include "DownloadService.h"
#include "StaticFileService.h"
#include "RequestAuthenticator.h"
#include "tudou/http/HttpServer.h"

#include <atomic>
#include <string>

namespace filelink {

class UploadService;

class ApiRouter {
    friend class TusControlApiTest;
public:
    ApiRouter(HttpServer& server, DownloadService& downloadService, StaticFileService& staticFileService,
        UploadService& uploadService, RequestAuthenticator& request_authenticator);

    void register_routes();

private:
    // 各个具体路由的处理函数
    void handle_health(const HttpRequest& req, HttpResponse& response);
    void handle_index(const HttpRequest& req, HttpResponse& response);
    void handle_static(const HttpRequest& req, HttpResponse& response);

    // Download
    void handle_download(const HttpRequest& req, HttpResponse& response);

    // Upload. 使用 Tus API Handlers 协议实现断点续传
    void handle_tus_options(const HttpRequest& req, HttpResponse& response);        // 功能协商。 OPTIONS /uploads
    void handle_tus_create(const HttpRequest& req, HttpResponse& response);         // 初始化创建上传会话。POST /uploads
    void handle_tus_patch(const HttpRequest& req, HttpResponse& response);          // 上传数据分片。PATCH /uploads/<uploadIdHex> 
    void handle_tus_head(const HttpRequest& req, HttpResponse& response);           // 查询上传进度/断点续传。HEAD /uploads/<uploadIdHex> 
    void handle_tus_get_session(const HttpRequest& req, HttpResponse& response);    // 获取会话状态，自定义的扩展接口，非 Tus 核心规范。GET /uploads/<uploadIdHex> 
    void handle_tus_terminate(const HttpRequest& req, HttpResponse& response);      // 主动取消会话。DELETE /uploads/<uploadIdHex> 
    bool authenticate_upload_request(const HttpRequest& req, AuthenticatedUser& out_user,
        HttpResponse& response);

private:
    HttpServer& server_;                    // 底层 HTTP 服务器实例
    DownloadService& downloadService_;      // 文件下载服务实例
    StaticFileService& staticFileService_;  // 静态资源服务实例
    UploadService& uploadService_;          // 文件上传服务实例
    RequestAuthenticator& requestAuthenticator_;
};

} // namespace filelink
