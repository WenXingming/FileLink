#include "site/SiteResponseView.h"
#include "site/StaticFileService.h"
#include "tudou/http/HttpResponse.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {

TEST(SiteResponseViewTest, BuildsHealthCheckResponse) {
    const HttpResponse response = filelink::SiteResponseView::health_check();

    EXPECT_EQ(response.get_status_code(), 200);
    EXPECT_EQ(response.get_status_message(), "OK");
    EXPECT_EQ(response.get_headers().at("Content-Type"), "application/json");
    EXPECT_EQ(response.get_body(), R"({"status":"ok"})");
}

TEST(SiteResponseViewTest, InfersAssetTypeFromPath) {
    const HttpResponse response = filelink::SiteResponseView::asset("/static/app.JS", "content");

    EXPECT_EQ(response.get_status_code(), 200);
    EXPECT_EQ(response.get_headers().at("Content-Type"), "application/javascript; charset=utf-8");
    EXPECT_EQ(response.get_headers().at("Content-Length"), "7");
    EXPECT_EQ(response.get_body(), "content");
}

TEST(SiteResponseViewTest, EscapesErrorMessage) {
    const HttpResponse response = filelink::SiteResponseView::forbidden("bad \"path\"\n");

    EXPECT_EQ(response.get_status_code(), 403);
    EXPECT_EQ(response.get_status_message(), "Forbidden");
    EXPECT_EQ(response.get_body(), R"({"status":"error","message":"bad \"path\"\n"})");
}

class StaticFileServiceTest : public testing::Test {
protected:
    void SetUp() override {
        web_root_ = "/tmp/filelink_site_test_" + std::to_string(getpid());
        ASSERT_EQ(::mkdir(web_root_.c_str(), 0700), 0);
    }

    void TearDown() override {
        std::remove((web_root_ + "/asset.txt").c_str());
        ::rmdir(web_root_.c_str());
    }

    std::string web_root_;
};

TEST_F(StaticFileServiceTest, ReadsAssetFromWebRoot) {
    std::ofstream file(web_root_ + "/asset.txt", std::ios::binary);
    file << "site asset";
    file.close();

    const filelink::StaticFileService service(web_root_);

    EXPECT_EQ(service.read_asset("/asset.txt"), "site asset");
}

TEST_F(StaticFileServiceTest, RejectsDirectoryTraversal) {
    const filelink::StaticFileService service(web_root_);

    EXPECT_THROW(service.read_asset("/../secret.txt"), std::invalid_argument);
}

} // namespace
