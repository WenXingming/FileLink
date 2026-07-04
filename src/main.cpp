#include "AppConfig.h"
#include "ObjectStore.h"
#include "ObjectService.h"
#include "StaticFileService.h"
#include "ApiRouter.h"
#include "tudou/http/HttpServer.h"

#include <exception>
#include <iostream>
#include <string>
#include <vector>
#include <sys/stat.h>

int main(int argc, char* argv[]) {
    try {
        const std::vector<std::string> args(argv + 1, argv + argc);
        const filelink::AppConfig config = filelink::parse_app_config(args);

        // 确保临时目录所在的根目录存在
        struct stat info;
        if (::stat(config.storageRoot.c_str(), &info) != 0) {
            ::mkdir(config.storageRoot.c_str(), 0755);
        }

        // 初始化对象存储与业务服务
        filelink::ObjectStore store(config.storageRoot);
        filelink::ObjectService objectService(std::move(store), config.storageRoot);
        filelink::StaticFileService staticFileService(config.webRoot);
        HttpServer server(config.listenAddress, config.port, config.ioThreads);
        
        // 挂载 API 路由模块
        filelink::ApiRouter router(server, objectService, staticFileService);
        router.register_routes();

        server.start();
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "filelink-server: " << error.what() << '\n';
        return 1;
    }
}
