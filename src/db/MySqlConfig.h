#pragma once

#include <cstdint>
#include <string>

namespace filelink {
namespace db {

// ================================================================
// MySqlConfig：建立 MySQL 连接池所需的连接与容量参数。
// ================================================================
struct MySqlConfig {
    std::string host = "127.0.0.1";
    uint16_t port = 3306;
    std::string user = "filelink";
    std::string password;
    std::string database = "filelink";
    unsigned int connectTimeoutSeconds = 5;
    unsigned int poolSize = 5;
};

} // namespace db
} // namespace filelink
