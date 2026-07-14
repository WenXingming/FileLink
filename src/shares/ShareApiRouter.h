#pragma once

#include "auth/RequestAuthenticator.h"
#include "tudou/http/HttpServer.h"
#include "tudou/http/HttpResponse.h"

namespace filelink {

class ShareService;
class FileService;

// ========================================================================
// ShareApiRouter：处理分享管理与公开下载 HTTP 请求。
// ========================================================================
class ShareApiRouter {
    friend class ShareApiTest;

public:
    ShareApiRouter(HttpServer& server, ShareService& share_service, FileService& file_service,
        RequestAuthenticator& request_authenticator)
        : server_(server), share_service_(share_service), file_service_(file_service),
          request_authenticator_(request_authenticator) {}

    void register_public_routes();

    // 请求属于分享管理接口时返回 true，并始终写入响应。
    bool handle_management_request(const HttpRequest& request, HttpResponse& response);

private:
    void handle_create(const HttpRequest& request, HttpResponse& response,
        const std::string& file_id);
    void handle_list(const HttpRequest& request, HttpResponse& response,
        const std::string& file_id);
    void handle_revoke(const HttpRequest& request, HttpResponse& response,
        const std::string& file_id, const std::string& share_id);
    void handle_public_download(const HttpRequest& request, HttpResponse& response);

    HttpServer& server_;
    ShareService& share_service_;
    FileService& file_service_;
    RequestAuthenticator& request_authenticator_;
};

} // namespace filelink
