#include "database/Share.h"
#include "shares/ShareRequestParser.h"
#include "shares/ShareResponseView.h"
#include "shares/ShareService.h"
#include "tudou/http/HttpRequest.h"
#include "tudou/http/HttpResponse.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <ctime>
#include <string>
#include <vector>

namespace {

TEST(ShareRequestParserTest, ParsesCollectionAndMemberPaths) {
    HttpRequest request;
    request.set_path("/shares/73686172652d6170692d66696c653031");

    std::string file_id;
    std::string share_id;
    EXPECT_TRUE(filelink::ShareRequestParser::parse_collection_file_id(request, file_id));
    EXPECT_EQ(file_id, "share-api-file01");

    request.set_path("/shares/73686172652d6170692d66696c653031/"
                     "73686172652d6170692d696430303031");
    EXPECT_TRUE(filelink::ShareRequestParser::parse_share_ids(request, file_id, share_id));
    EXPECT_EQ(file_id, "share-api-file01");
    EXPECT_EQ(share_id, "share-api-id0001");
}

TEST(ShareRequestParserTest, ParsesExpiry) {
    HttpRequest request;
    request.set_body(R"({"expires_in_seconds":3600})");

    std::time_t expiry;
    const std::time_t earliest_expiry = std::time(nullptr) + 3600;

    EXPECT_TRUE(filelink::ShareRequestParser::parse_expiry(request, expiry));
    EXPECT_GE(expiry, earliest_expiry);
}

TEST(ShareRequestParserTest, RejectsMalformedInput) {
    HttpRequest request;
    request.set_path("/shares/73686172652D6170692D66696C653031");
    request.set_body(R"({"expires_in_seconds":0})");

    std::string file_id;
    std::time_t expiry;
    EXPECT_FALSE(filelink::ShareRequestParser::parse_collection_file_id(request, file_id));
    EXPECT_FALSE(filelink::ShareRequestParser::parse_expiry(request, expiry));
}

TEST(ShareResponseViewTest, BuildsCreatedAndListResponses) {
    const std::time_t expiry = 1700000000;
    std::tm expiry_time{};
    localtime_r(&expiry, &expiry_time);
    const filelink::CreatedShare created{
        "share-api-id0001", "public-token", expiry_time
    };
    const std::vector<filelink::db::Share> shares = {
        {"share-api-id0001", "share-api-file01", std::string(32, 'a'), expiry_time, {}}
    };

    const HttpResponse created_response = filelink::ShareResponseView::created(created);
    const HttpResponse list_response = filelink::ShareResponseView::share_list(shares);
    const nlohmann::json created_body = nlohmann::json::parse(created_response.get_body());
    const nlohmann::json list_body = nlohmann::json::parse(list_response.get_body());

    EXPECT_EQ(created_response.get_status_code(), 201);
    EXPECT_EQ(created_body.at("share_id"), "73686172652d6170692d696430303031");
    EXPECT_EQ(created_body.at("token"), "public-token");
    ASSERT_EQ(list_body.at("shares").size(), 1u);
    EXPECT_EQ(list_body.at("shares")[0].at("share_id"), created_body.at("share_id"));
}

TEST(ShareResponseViewTest, BuildsCommonResponses) {
    EXPECT_EQ(filelink::ShareResponseView::invalid_expiry().get_status_code(), 400);
    EXPECT_EQ(filelink::ShareResponseView::unauthorized().get_status_code(), 401);
    EXPECT_EQ(filelink::ShareResponseView::not_found().get_status_code(), 404);
    EXPECT_EQ(filelink::ShareResponseView::revoked().get_status_code(), 204);
    EXPECT_EQ(filelink::ShareResponseView::server_error("Share listing failed").get_status_code(), 500);
}

} // namespace
