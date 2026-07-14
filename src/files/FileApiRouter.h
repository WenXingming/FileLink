#pragma once

#include "auth/RequestAuthenticator.h"
#include "tudou/http/HttpServer.h"

namespace filelink {

class FileService;
class ShareApiRouter;

// ====================================================================
// FileApiRouter：提供当前用户私有文件库的 HTTP 接口。
// ====================================================================
class FileApiRouter {
    friend class FileApiTest;

public:
    FileApiRouter(HttpServer& server, FileService& file_service,
        RequestAuthenticator& request_authenticator, ShareApiRouter& share_api_router)
        : server_(server), file_service_(file_service), request_authenticator_(request_authenticator),
          share_api_router_(share_api_router) {}

    void register_routes();

private:
    void handle_list_files(const HttpRequest& request, HttpResponse& response);
    void handle_download(const HttpRequest& request, HttpResponse& response);
    void handle_delete(const HttpRequest& request, HttpResponse& response);

    HttpServer& server_;
    FileService& file_service_;
    RequestAuthenticator& request_authenticator_;
    ShareApiRouter& share_api_router_;
};

} // namespace filelink
