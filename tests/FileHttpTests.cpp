#include "database/File.h"
#include "files/FileRequestParser.h"
#include "files/FileResponseView.h"
#include "tudou/http/HttpRequest.h"
#include "tudou/http/HttpResponse.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace {

TEST(FileRequestParserTest, ParsesDownloadFileId) {
    HttpRequest request;
    request.set_method("GET");
    request.set_path("/files/646f776e6c6f61642d66696c65303030/download");

    std::string file_id;

    EXPECT_TRUE(filelink::FileRequestParser::parse_download_file_id(request, file_id));
    EXPECT_EQ(file_id, "download-file000");
}

TEST(FileRequestParserTest, ParsesFileId) {
    HttpRequest request;
    request.set_method("DELETE");
    request.set_path("/files/616c6963652d66696c652d6964303031");

    std::string file_id;

    EXPECT_TRUE(filelink::FileRequestParser::parse_file_id(request, file_id));
    EXPECT_EQ(file_id, "alice-file-id001");
}

TEST(FileRequestParserTest, RejectsMalformedFilePath) {
    HttpRequest request;
    request.set_method("GET");
    request.set_path("/files/646F776E6C6F61642D66696C65303030/download");

    std::string file_id;

    EXPECT_FALSE(filelink::FileRequestParser::parse_download_file_id(request, file_id));
}

TEST(FileResponseViewTest, BuildsFileList) {
    const std::vector<filelink::db::File> files = {
        {"alice-file-id001", "alice-user-id001", std::string(32, 'a'), "report.txt", {}}
    };

    const HttpResponse response = filelink::FileResponseView::file_list(files);
    const nlohmann::json body = nlohmann::json::parse(response.get_body());

    EXPECT_EQ(response.get_status_code(), 200);
    ASSERT_EQ(body.at("files").size(), 1u);
    EXPECT_EQ(body.at("files")[0].at("file_id"), "616c6963652d66696c652d6964303031");
    EXPECT_EQ(body.at("files")[0].at("name"), "report.txt");
}

TEST(FileResponseViewTest, BuildsCommonResponses) {
    const HttpResponse unauthorized = filelink::FileResponseView::unauthorized();
    const HttpResponse not_found = filelink::FileResponseView::not_found();
    const HttpResponse deleted = filelink::FileResponseView::deleted();
    const HttpResponse failure = filelink::FileResponseView::server_error("File deletion failed");

    EXPECT_EQ(unauthorized.get_status_code(), 401);
    EXPECT_EQ(unauthorized.get_body(), R"({"message":"Unauthorized"})");
    EXPECT_EQ(not_found.get_status_code(), 404);
    EXPECT_EQ(not_found.get_body(), R"({"message":"Not Found"})");
    EXPECT_EQ(deleted.get_status_code(), 204);
    EXPECT_TRUE(deleted.get_body().empty());
    EXPECT_EQ(failure.get_status_code(), 500);
    EXPECT_EQ(failure.get_body(), R"({"message":"File deletion failed"})");
}

} // namespace
