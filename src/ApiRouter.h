#pragma once

#include "LocalObjectStore.h"
#include "tudou/http/HttpServer.h"

#include <atomic>
#include <string>

namespace filelink {

class ApiRouter {
public:
    ApiRouter(HttpServer& server, LocalObjectStore store, std::string storageRoot);

    void registerRoutes();

private:
    // 各个具体路由的处理函数
    void handleHealth(const HttpRequest& req, HttpResponse& response);
    void handleUpload(const HttpRequest& req, HttpResponse& response);

private:
    HttpServer& server_;
    LocalObjectStore store_;
    std::string storageRoot_;
    std::atomic<uint64_t> reqCounter_{ 0 };
};

} // namespace filelink
