#pragma once

#include "auth/RequestAuthenticator.h"
#include "tudou/http/HttpResponse.h"

namespace filelink {

class ShareService;

// ========================================================================
// ShareApiRouter：处理文件分享链接的创建、查看和撤销 HTTP 请求。
// ========================================================================
class ShareApiRouter {
    friend class ShareApiTest;

public:
    ShareApiRouter(ShareService& share_service, RequestAuthenticator& request_authenticator)
        : share_service_(share_service), request_authenticator_(request_authenticator) {}

    // 请求属于分享管理接口时返回 true，并始终写入响应。
    bool handle_request(const HttpRequest& request, HttpResponse& response);

private:
    void handle_create(const HttpRequest& request, HttpResponse& response,
        const std::string& file_id);
    void handle_list(const HttpRequest& request, HttpResponse& response,
        const std::string& file_id);
    void handle_revoke(const HttpRequest& request, HttpResponse& response,
        const std::string& file_id, const std::string& share_id);

    ShareService& share_service_;
    RequestAuthenticator& request_authenticator_;
};

} // namespace filelink
