#pragma once

#include <cstdlib>
#include <stdexcept>
#include <string>

namespace filelink {
namespace test {

inline std::string mysql_connection_string() {
    const char* password = std::getenv("FILELINK_TEST_MYSQL_PASSWORD");
    if (password == nullptr || password[0] == '\0') {
        throw std::runtime_error(
            "集成测试需要设置 FILELINK_TEST_MYSQL_PASSWORD");
    }

    const char* port = std::getenv("FILELINK_TEST_MYSQL_PORT");
    return "db=filelink user=filelink password=" + std::string(password)
        + " host=127.0.0.1 port=" + (port == nullptr ? "3306" : port);
}

} // namespace test
} // namespace filelink
