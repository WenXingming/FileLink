#include "MySqlTestConfig.h"
#include "auth/AuthService.h"
#include "auth/RequestAuthenticator.h"
#include "db/File.h"
#include "db/Object.h"
#include "files/FileApiRouter.h"
#include "files/FileService.h"
#include "tudou/http/HttpRequest.h"
#include "tudou/http/HttpResponse.h"
#include "tudou/http/HttpServer.h"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <soci/mysql/soci-mysql.h>

namespace filelink {

class FileApiTest : public testing::Test {
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
        sql << "DELETE FROM files";
        sql << "DELETE FROM objects";
        sql << "DELETE FROM user_sessions";
        sql << "DELETE FROM users";
    }

    HttpResponse list_files(FileApiRouter& router, const std::string& token) {
        HttpRequest request;
        request.set_method("GET");
        request.set_path("/files");
        if (!token.empty()) {
            request.add_header("Cookie", "filelink_session=" + token);
        }

        HttpResponse response;
        router.handle_list_files(request, response);
        return response;
    }

    soci::connection_pool pool_{1};
};

TEST_F(FileApiTest, ListsOnlyCurrentUsersFiles) {
    AuthService auth_service(pool_);
    AuthenticatedSession alice;
    AuthenticatedSession bob;
    ASSERT_EQ(auth_service.register_user("alice", "correct-password", alice), RegisterResult::Success);
    ASSERT_EQ(auth_service.register_user("bob", "correct-password", bob), RegisterResult::Success);

    {
        soci::session sql(pool_);
        const std::string content_hash = "12345678901234567890123456789012";
        db::ObjectDao(sql).add_reference(content_hash, 10);
        db::FileDao(sql).create({"alice-file-id001", alice.user_id, content_hash, "alice.txt", {}});
        db::FileDao(sql).create({"bob-file-id00001", bob.user_id, content_hash, "bob.txt", {}});
    }

    HttpServer server("127.0.0.1", 9999);
    RequestAuthenticator request_authenticator(auth_service);
    FileService file_service(pool_);
    FileApiRouter router(server, file_service, request_authenticator);

    const HttpResponse response = list_files(router, alice.session_token);
    ASSERT_EQ(response.get_status_code(), 200);
    const nlohmann::json body = nlohmann::json::parse(response.get_body());
    ASSERT_EQ(body.at("files").size(), 1u);
    EXPECT_EQ(body.at("files")[0].at("name"), "alice.txt");
    EXPECT_EQ(body.at("files")[0].at("file_id"), "616c6963652d66696c652d6964303031");
}

TEST_F(FileApiTest, RejectsUnauthenticatedRequests) {
    AuthService auth_service(pool_);
    RequestAuthenticator request_authenticator(auth_service);
    FileService file_service(pool_);
    HttpServer server("127.0.0.1", 9999);
    FileApiRouter router(server, file_service, request_authenticator);

    const HttpResponse response = list_files(router, "");
    EXPECT_EQ(response.get_status_code(), 401);
    EXPECT_EQ(response.get_body(), R"({"message":"Unauthorized"})");
}

} // namespace filelink
