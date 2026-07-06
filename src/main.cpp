#include "AppConfig.h"
#include "ObjectStore.h"
#include "DownloadService.h"
#include "StaticFileService.h"
#include "ApiRouter.h"
#include "UploadService.h"
#include "cleaner/SessionCleaner.h"
#include "tudou/http/HttpServer.h"
#include <soci/soci.h>
#include <soci/connection-pool.h>
#include <soci/mysql/soci-mysql.h>

#include <exception>
#include <iostream>
#include <string>
#include <vector>
#include <sys/stat.h>

int main(int argc, char* argv[]) {
    try {
        const std::vector<std::string> args(argv + 1, argv + argc);
        const filelink::AppConfig config = filelink::parse_app_config(args);
        filelink::validate_app_config(config);

        // 确保临时目录所在的根目录存在
        struct stat info;
        if (::stat(config.storageRoot.c_str(), &info) != 0) {
            ::mkdir(config.storageRoot.c_str(), 0755);
        }

        // 初始化对象存储与业务服务
        filelink::DownloadService downloadService(filelink::ObjectStore(config.storageRoot));
        filelink::StaticFileService staticFileService(config.webRoot);
        HttpServer server(config.listenAddress, config.port, config.ioThreads);

        // 初始化 SOCI 数据库连接池
        std::size_t poolSize = config.mysql.poolSize > 0 ? config.mysql.poolSize : 5;
        soci::connection_pool mysqlPool(poolSize);
        std::string connStr = "db=" + config.mysql.database +
                              " user=" + config.mysql.user +
                              " password=" + config.mysql.password +
                              " host=" + config.mysql.host +
                              " port=" + std::to_string(config.mysql.port);
        for (std::size_t i = 0; i < poolSize; ++i) {
            mysqlPool.at(i).open(soci::mysql, connStr);
        }

        if (config.cleanupExpiredOnly) {
            filelink::SessionCleaner cleaner(mysqlPool, config.storageRoot);
            int count = cleaner.cleanup_expired_sessions();
            std::cout << "Successfully cleaned " << count << " expired sessions.\n";
            return 0;
        }

        // 初始化上传业务服务
        filelink::UploadService uploadService(mysqlPool, config.storageRoot, filelink::ObjectStore(config.storageRoot));
        
        // 挂载 API 路由模块
        filelink::ApiRouter router(server, downloadService, staticFileService, uploadService);
        router.register_routes();

        server.start();
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "filelink-server: " << error.what() << '\n';
        return 1;
    }
}
