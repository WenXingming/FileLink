#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "database/MySqlConfig.h"
#include "redis/RedisConfig.h"

namespace filelink {

// ===================================================================
// AppConfig：服务进程启动时使用的 HTTP、存储与数据库配置。
// ===================================================================
struct AppConfig {
    std::string listenAddress = "0.0.0.0";
    uint16_t port = 8080;
    int ioThreads = 0;
    std::string storageRoot = "./storage";
    std::string webRoot = "./web";
    std::string logRoot = "./logs";
    bool cleanupExpiredOnly = false;
    bool reclaimPendingObjectsOnly = false;
    bool scanOrphanedObjectsOnly = false;
    bool reclaimOrphanedObjectsOnly = false;

    db::MySqlConfig mysql;
    redis::RedisConfig redis;
};

AppConfig parse_app_config(const std::vector<std::string>& args);
void validate_app_config(const AppConfig& config);

} // namespace filelink
