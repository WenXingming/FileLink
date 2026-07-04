#include "MySqlConnection.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

filelink::MySqlConfig test_options() {
    filelink::MySqlConfig options;
    options.user = "filelink";
    options.database = "filelink";

    const char* password = std::getenv("FILELINK_TEST_MYSQL_PASSWORD");
    if (password != nullptr) {
        options.password = password;
    }
    const char* port = std::getenv("FILELINK_TEST_MYSQL_PORT");
    if (port != nullptr) {
        options.port = static_cast<uint16_t>(std::stoi(port));
    }
    return options;
}

bool mysql_test_enabled() {
    return std::getenv("FILELINK_TEST_MYSQL_PASSWORD") != nullptr;
}

} // namespace

TEST(MySqlConnectionTest, RejectsInvalidOptionsBeforeConnecting) {
    filelink::MySqlConfig options;
    options.host = "";
    options.user = "";
    options.database = "";

    EXPECT_THROW(filelink::MySqlConnection connection(options), std::invalid_argument);

    options.user = "filelink";
    EXPECT_THROW(filelink::MySqlConnection connection(options), std::invalid_argument);
    EXPECT_THROW(filelink::MySqlConnection connection(options), std::invalid_argument);
}

TEST(MySqlConnectionTest, ConnectsAndPingsServer) {
    if (!mysql_test_enabled()) {
        GTEST_SKIP() << "未设置 FILELINK_TEST_MYSQL_PASSWORD";
    }

    filelink::MySqlConnection connection(test_options());
    EXPECT_TRUE(connection.ping());

    filelink::MySqlConnection moved(std::move(connection));
    EXPECT_TRUE(moved.ping());
}

TEST(MySqlConnectionTest, DoesNotExposePasswordOnAuthenticationFailure) {
    if (!mysql_test_enabled()) {
        GTEST_SKIP() << "未设置 FILELINK_TEST_MYSQL_PASSWORD";
    }

    filelink::MySqlConfig options = test_options();
    options.password += "-invalid";

    try {
        filelink::MySqlConnection connection(options);
        FAIL() << "错误密码不应连接成功";
    } catch (const std::runtime_error& error) {
        EXPECT_EQ(std::string(error.what()).find(options.password), std::string::npos);
    }
}
