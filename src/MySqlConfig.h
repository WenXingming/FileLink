#pragma once

#include <cstdint>
#include <string>

namespace filelink {

struct MySqlConfig {
    std::string host = "127.0.0.1";
    uint16_t port = 3306;
    std::string user = "filelink";
    std::string password;
    std::string database = "filelink";
    unsigned int connectTimeoutSeconds = 5;
};

} // namespace filelink
