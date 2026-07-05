#include "AppConfig.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace {

class AppConfigTest : public testing::Test {
protected:
    void SetUp() override {
        configPath_ = "/tmp/filelink_app_config_" + std::to_string(getpid()) + ".toml";
    }

    void TearDown() override {
        std::remove(configPath_.c_str());
    }

    void write_config(const std::string& content) const {
        std::ofstream output(configPath_.c_str());
        ASSERT_TRUE(output.is_open());
        output << content;
    }

    std::string configPath_;
};

} // namespace

TEST_F(AppConfigTest, UsesDefaults) {
    const filelink::AppConfig config = filelink::parse_app_config({});

    EXPECT_EQ(config.listenAddress, "0.0.0.0");
    EXPECT_EQ(config.port, 8080);
    EXPECT_EQ(config.ioThreads, 0);
    EXPECT_EQ(config.storageRoot, "./storage");
    EXPECT_EQ(config.webRoot, "./web");
    EXPECT_EQ(config.logRoot, "./logs");
}

TEST_F(AppConfigTest, ReadsCommandLineOptions) {
    const filelink::AppConfig config = filelink::parse_app_config({
        "--address", "127.0.0.1",
        "--port", "9000",
        "--io-threads", "4",
        "--storage-root", "/srv/filelink"
    });

    EXPECT_EQ(config.listenAddress, "127.0.0.1");
    EXPECT_EQ(config.port, 9000);
    EXPECT_EQ(config.ioThreads, 4);
    EXPECT_EQ(config.storageRoot, "/srv/filelink");
}

TEST_F(AppConfigTest, ReadsTomlConfig) {
    write_config(
        "address = \"127.0.0.1\"\n"
        "port = 9001\n"
        "io-threads = 3\n"
        "storage-root = \"/data/filelink\"\n"
    );

    const filelink::AppConfig config =
        filelink::parse_app_config({"--config", configPath_});

    EXPECT_EQ(config.listenAddress, "127.0.0.1");
    EXPECT_EQ(config.port, 9001);
    EXPECT_EQ(config.ioThreads, 3);
    EXPECT_EQ(config.storageRoot, "/data/filelink");
}

TEST_F(AppConfigTest, CommandLineOverridesTomlConfig) {
    write_config(
        "port = 9001\n"
        "io-threads = 3\n"
    );

    const filelink::AppConfig config = filelink::parse_app_config({
        "--config", configPath_,
        "--port", "9100"
    });

    EXPECT_EQ(config.port, 9100);
    EXPECT_EQ(config.ioThreads, 3);
}

TEST_F(AppConfigTest, RejectsInvalidNumbers) {
    EXPECT_ANY_THROW(filelink::parse_app_config({"--port", "0"}));
    EXPECT_ANY_THROW(filelink::parse_app_config({"--port", "65536"}));
    EXPECT_ANY_THROW(filelink::parse_app_config({"--io-threads", "-1"}));
}

TEST_F(AppConfigTest, RejectsInvalidInput) {
    EXPECT_ANY_THROW(filelink::parse_app_config({"--storage-root", ""}));
    EXPECT_ANY_THROW(filelink::parse_app_config({"--unknown-option"}));
    EXPECT_ANY_THROW(filelink::parse_app_config({
        "--config", "/tmp/filelink_missing_config.toml"
    }));
}

TEST(AppConfigValidationTest, RejectsMissingMysqlPassword) {
    filelink::AppConfig config;

    try {
        filelink::validate_app_config(config);
        FAIL() << "缺少 MySQL 密码时应拒绝启动";
    } catch (const std::invalid_argument& error) {
        EXPECT_STREQ(
            error.what(),
            "缺少必需配置 FILELINK_MYSQL_PASSWORD，请通过环境变量设置 MySQL 密码");
    }
}
