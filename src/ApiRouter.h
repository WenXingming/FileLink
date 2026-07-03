#pragma once

#include "LocalObjectStore.h"
#include "tudou/http/HttpServer.h"

#include <atomic>
#include <string>

namespace filelink {

class ApiRouter {
public:
    /**
     * @brief 构造路由控制器
     * @param server 底层 HTTP 服务器实例
     * @param store 本地对象存储实例
     * @param storageRoot 存储根路径（用于生成临时文件）
     * @param webRoot 静态资源根路径
     */
    ApiRouter(HttpServer& server, LocalObjectStore store, std::string storageRoot, std::string webRoot);

    /**
     * @brief 注册所有支持的 API 路由
     */
    void registerRoutes();

private:
    // 各个具体路由的处理函数
    void handleIndex(const HttpRequest& req, HttpResponse& response);
    void handleHealth(const HttpRequest& req, HttpResponse& response);
    void handleUpload(const HttpRequest& req, HttpResponse& response);
    void handleDownload(const HttpRequest& req, HttpResponse& response);

private:
    HttpServer& server_;
    LocalObjectStore store_;
    std::string storageRoot_;
    std::string webRoot_;
    std::atomic<uint64_t> reqCounter_{ 0 };
};

} // namespace filelink
