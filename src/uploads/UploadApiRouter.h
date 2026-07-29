#pragma once

#include "tudou/http/HttpServer.h"
#include "uploads/UploadService.h"

namespace filelink {

class AuthService;
struct AuthenticatedUser;

// =====================================================================
// UploadApiRouter：提供需要账户认证的 Tus 上传 HTTP 接口。
// =====================================================================
class UploadApiRouter {
    friend class TusControlApiTest;
public:
    UploadApiRouter(HttpServer& server, UploadService& upload_service,
        AuthService& auth_service);

    void register_routes();

private:
    void handle_tus_options(const HttpRequest& req, HttpResponse& response);
    void handle_tus_create(const HttpRequest& req, HttpResponse& response);
    void handle_tus_patch(const HttpRequest& req, HttpResponse& response);
    void handle_tus_head(const HttpRequest& req, HttpResponse& response);
    void handle_tus_get_session(const HttpRequest& req, HttpResponse& response);
    void handle_tus_terminate(const HttpRequest& req, HttpResponse& response);
    bool authenticate_upload_request(const HttpRequest& req, AuthenticatedUser& out_user,
        HttpResponse& response);

    HttpServer& server_;
    UploadService& uploadService_;
    AuthService& auth_service_;
};

} // namespace filelink
