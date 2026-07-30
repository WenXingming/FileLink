// ============================================================================
// Downloads HTTP 单元测试：验证下载路径解析与 Nginx 内部重定向响应。
// 不依赖数据库、磁盘文件或正在运行的服务器。
// ============================================================================

#include "downloads/DownloadRequestParser.h"
#include "downloads/DownloadResponseView.h"
#include "downloads/DownloadService.h"
#include "tudou/http/HttpRequest.h"
#include "tudou/http/HttpResponse.h"

#include <gtest/gtest.h>

#include <string>

namespace {

TEST(DownloadRequestParserTest, ParsesPrivateFileId) {
    HttpRequest request;
    request.set_path("/downloads/private/646f776e6c6f61642d66696c65303030");

    std::string file_id;

    EXPECT_TRUE(filelink::DownloadRequestParser::parse_private_file_id(request, file_id));
    EXPECT_EQ(file_id, "download-file000");
}

TEST(DownloadRequestParserTest, ParsesSharedToken) {
    HttpRequest request;
    request.set_path("/downloads/shared/" + std::string(64, 'a'));

    std::string token;

    EXPECT_TRUE(filelink::DownloadRequestParser::parse_shared_token(request, token));
    EXPECT_EQ(token, std::string(64, 'a'));
}

TEST(DownloadRequestParserTest, RejectsMalformedPaths) {
    HttpRequest request;
    std::string value;

    request.set_path("/downloads/private/646F776E6C6F61642D66696C65303030");
    EXPECT_FALSE(filelink::DownloadRequestParser::parse_private_file_id(request, value));

    request.set_path("/downloads/shared/" + std::string(63, 'a'));
    EXPECT_FALSE(filelink::DownloadRequestParser::parse_shared_token(request, value));
}

TEST(DownloadResponseViewTest, BuildsInternalRedirect) {
    const filelink::DownloadTarget target{
        "aa/aa/" + std::string(64, 'a'), "report.pdf"
    };

    const HttpResponse response = filelink::DownloadResponseView::file(target);

    EXPECT_EQ(response.get_status_code(), 200);
    EXPECT_TRUE(response.get_body().empty());
    EXPECT_EQ(response.get_headers().at("X-Accel-Redirect"),
        "/_filelink_objects/aa/aa/" + std::string(64, 'a'));
    EXPECT_EQ(response.get_headers().at("Content-Disposition"),
        "attachment; filename=\"report.pdf\"");
}

TEST(DownloadResponseViewTest, BuildsCommonErrors) {
    EXPECT_EQ(filelink::DownloadResponseView::unauthorized().get_status_code(), 401);
    EXPECT_EQ(filelink::DownloadResponseView::not_found().get_status_code(), 404);
    EXPECT_EQ(filelink::DownloadResponseView::server_error("Download failed").get_status_code(),
        500);
}

} // namespace
