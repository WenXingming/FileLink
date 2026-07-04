#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "MySqlConfig.h"

namespace filelink {

struct AppConfig {
    std::string listenAddress = "0.0.0.0";
    uint16_t port = 8080;
    int ioThreads = 0;
    std::string storageRoot = "./storage";
    std::string webRoot = "./web";
    std::string logRoot = "./logs";

    MySqlConfig mysql;
};

AppConfig parse_app_config(const std::vector<std::string>& args);

} // namespace filelink
