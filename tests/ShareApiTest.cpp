#include "MySqlTestConfig.h"
#include "ObjectStore.h"
#include "auth/AuthService.h"
#include "auth/RequestAuthenticator.h"
#include "db/File.h"
#include "db/Object.h"
#include "files/FileService.h"
#include "shares/ShareApiRouter.h"
#include "shares/ShareService.h"
#include "tudou/http/HttpRequest.h"
#include "tudou/http/HttpResponse.h"
#include "tudou/http/HttpServer.h"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <soci/mysql/soci-mysql.h>

#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

namespace filelink {

class ShareApiTest : public testing::Test {
protected:
    void SetUp() override {
        pool_.at(0).open(soci::mysql, test::mysql_connection_string());
        clear_database();
    }

    void TearDown() override {
        clear_database();
    }

    void clear_database() {
        soci::session sql(pool_);
        sql << "DELETE FROM upload_sessions";
        sql << "DELETE FROM shares";
        sql << "DELETE FROM files";
        sql << "DELETE FROM objects";
        sql << "DELETE FROM user_sessions";
        sql << "DELETE FROM users";
    }

    HttpResponse request(ShareApiRouter& router, const std::string& method,
        const std::string& path, const std::string& token, const std::string& body = "") {
        HttpRequest request;
        request.set_method(method);
        request.set_path(path);
        request.set_body(body);
        if (!token.empty()) {
            request.add_header("Cookie", "filelink_session=" + token);
        }

        HttpResponse response;
        EXPECT_TRUE(router.handle_management_request(request, response));
        return response;
    }

    HttpResponse download(ShareApiRouter& router, const std::string& token) {
        HttpRequest request;
        request.set_method("GET");
        request.set_path("/shares/" + token + "/download");

        HttpResponse response;
        router.handle_public_download(request, response);
        return response;
    }

    soci::connection_pool pool_{1};
};

TEST_F(ShareApiTest, OwnersCanCreateListAndRevokeShares) {
    AuthService auth_service(pool_);
    AuthenticatedSession alice;
    AuthenticatedSession bob;
    ASSERT_EQ(auth_service.register_user("alice", "correct-password", alice), RegisterResult::Success);
    ASSERT_EQ(auth_service.register_user("bob", "correct-password", bob), RegisterResult::Success);

    const std::string file_id = "share-api-file01";
    const std::string file_id_hex = "73686172652d6170692d66696c653031";
    {
        soci::session sql(pool_);
        db::ObjectDao(sql).add_reference(std::string(32, 'a'), 42);
        db::FileDao(sql).create({file_id, alice.user_id, std::string(32, 'a'), "report.pdf", {}});
    }

    RequestAuthenticator request_authenticator(auth_service);
    ShareService share_service(pool_);
    FileService file_service(pool_, ObjectStore("./storage_test"));
    HttpServer server("127.0.0.1", 9999);
    ShareApiRouter router(server, share_service, file_service, request_authenticator);
    const std::string collection_path = "/files/" + file_id_hex + "/shares";

    const HttpResponse created = request(router, "POST", collection_path, alice.session_token,
        R"({"expires_in_seconds":3600})");
    ASSERT_EQ(created.get_status_code(), 201);
    const nlohmann::json created_body = nlohmann::json::parse(created.get_body());
    EXPECT_EQ(created_body.at("token").get<std::string>().size(), 64u);
    const std::string share_id = created_body.at("share_id").get<std::string>();
    EXPECT_EQ(share_id.size(), 32u);

    const HttpResponse listed = request(router, "GET", collection_path, alice.session_token);
    ASSERT_EQ(listed.get_status_code(), 200);
    const nlohmann::json listed_body = nlohmann::json::parse(listed.get_body());
    ASSERT_EQ(listed_body.at("shares").size(), 1u);
    EXPECT_EQ(listed_body.at("shares")[0].at("share_id"), share_id);

    EXPECT_EQ(request(router, "GET", collection_path, bob.session_token).get_status_code(), 404);
    EXPECT_EQ(request(router, "DELETE", collection_path + "/" + share_id,
        bob.session_token).get_status_code(), 404);
    EXPECT_EQ(request(router, "DELETE", collection_path + "/" + share_id,
        alice.session_token).get_status_code(), 204);

    const HttpResponse empty_list = request(router, "GET", collection_path, alice.session_token);
    ASSERT_EQ(empty_list.get_status_code(), 200);
    EXPECT_TRUE(nlohmann::json::parse(empty_list.get_body()).at("shares").empty());
}

TEST_F(ShareApiTest, RejectsUnauthenticatedAndInvalidExpiryRequests) {
    AuthService auth_service(pool_);
    RequestAuthenticator request_authenticator(auth_service);
    ShareService share_service(pool_);
    FileService file_service(pool_, ObjectStore("./storage_test"));
    HttpServer server("127.0.0.1", 9999);
    ShareApiRouter router(server, share_service, file_service, request_authenticator);
    const std::string path = "/files/73686172652d6170692d66696c653031/shares";

    EXPECT_EQ(request(router, "POST", path, "", R"({"expires_in_seconds":3600})").get_status_code(), 401);

    AuthenticatedSession alice;
    ASSERT_EQ(auth_service.register_user("alice", "correct-password", alice), RegisterResult::Success);
    EXPECT_EQ(request(router, "POST", path, alice.session_token,
        R"({"expires_in_seconds":0})").get_status_code(), 400);
}

TEST_F(ShareApiTest, DownloadsFilesGrantedByActiveTokenWithoutAuthentication) {
    AuthService auth_service(pool_);
    AuthenticatedSession alice;
    ASSERT_EQ(auth_service.register_user("alice", "correct-password", alice), RegisterResult::Success);

    const std::string content = "shared file content";
    const std::string content_hash(32, static_cast<char>(0xaa));
    ObjectStore store("./storage_test");
    ::mkdir("./storage_test", 0755);
    const std::string temporary_path = "./storage_test/share_api_download.tmp";
    {
        std::ofstream output(temporary_path, std::ios::binary);
        output << content;
    }
    const CommitResult committed = store.commit(temporary_path, std::string(64, 'a'));
    {
        soci::session sql(pool_);
        db::ObjectDao(sql).add_reference(content_hash, content.size());
        db::FileDao(sql).create({"share-api-file01", alice.user_id, content_hash, "report.pdf", {}});
    }

    ShareService share_service(pool_);
    CreatedShare share;
    ASSERT_EQ(share_service.create_share(alice.user_id, "share-api-file01",
            std::time(nullptr) + 3600, share),
        CreateShareResult::Success);

    RequestAuthenticator request_authenticator(auth_service);
    FileService file_service(pool_, store);
    HttpServer server("127.0.0.1", 9999);
    ShareApiRouter router(server, share_service, file_service, request_authenticator);
    const HttpResponse response = download(router, share.token);

    EXPECT_EQ(response.get_status_code(), 200);
    EXPECT_TRUE(response.has_file_body());
    EXPECT_EQ(response.get_file_size(), content.size());
    EXPECT_EQ(response.get_headers().at("Content-Disposition"), "attachment; filename=\"report.pdf\"");

    ASSERT_EQ(share_service.revoke_share(alice.user_id, "share-api-file01", share.share_id),
        RevokeShareResult::Success);
    const HttpResponse revoked_response = download(router, share.token);
    EXPECT_EQ(revoked_response.get_status_code(), 404);

    ::unlink(committed.objectPath.c_str());
}

} // namespace filelink
