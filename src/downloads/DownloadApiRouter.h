// ============================================================================
// Downloads HTTP Controller：注册私有下载和分享下载接口。
// 只编排请求解析、私有请求认证、DownloadService 和响应 View。
// ============================================================================

#pragma once

class HttpRequest;
class HttpResponse;
class HttpServer;

namespace filelink {

class AuthService;
class DownloadService;

class DownloadApiRouter {
public:
    DownloadApiRouter(HttpServer& server, DownloadService& download_service, AuthService& auth_service);

    void register_routes();

private:
    void handle_private_download(const HttpRequest& request, HttpResponse& response);
    void handle_shared_download(const HttpRequest& request, HttpResponse& response);

private:
    HttpServer& server_;
    DownloadService& download_service_;
    AuthService& auth_service_;
};

} // namespace filelink
