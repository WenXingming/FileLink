#pragma once

#include "tudou/http/HttpServer.h"

namespace filelink {

class AuthService;
class FileService;
class ObjectStore;
class ShareApiRouter;

// ====================================================================
// FileApiRouter：提供当前用户私有文件库的 HTTP 接口。
// ====================================================================
class FileApiRouter {
    friend class FileApiTest;

public:
    FileApiRouter(HttpServer& server, FileService& file_service, const ObjectStore& object_store,
        AuthService& auth_service, ShareApiRouter& share_api_router)
        : server_(server), file_service_(file_service), object_store_(object_store),
          auth_service_(auth_service), share_api_router_(share_api_router) {}

    void register_routes();

private:
    void handle_list_files(const HttpRequest& request, HttpResponse& response);
    void handle_download(const HttpRequest& request, HttpResponse& response);
    void handle_delete(const HttpRequest& request, HttpResponse& response);

    HttpServer& server_;
    FileService& file_service_;
    const ObjectStore& object_store_;
    AuthService& auth_service_;
    ShareApiRouter& share_api_router_;
};

} // namespace filelink
