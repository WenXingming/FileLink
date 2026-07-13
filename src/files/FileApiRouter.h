#pragma once

#include "auth/RequestAuthenticator.h"
#include "tudou/http/HttpServer.h"

namespace filelink {

class FileService;

// ====================================================================
// FileApiRouter：提供当前用户私有文件库的 HTTP 接口。
// ====================================================================
class FileApiRouter {
    friend class FileApiTest;

public:
    FileApiRouter(HttpServer& server, FileService& file_service,
        RequestAuthenticator& request_authenticator)
        : server_(server), file_service_(file_service), request_authenticator_(request_authenticator) {}

    void register_routes();

private:
    void handle_list_files(const HttpRequest& request, HttpResponse& response);

    HttpServer& server_;
    FileService& file_service_;
    RequestAuthenticator& request_authenticator_;
};

} // namespace filelink
