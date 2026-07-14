#include "MySqlTestConfig.h"
#include "ObjectStore.h"
#include "auth/AuthService.h"
#include "auth/RequestAuthenticator.h"
#include "db/File.h"
#include "db/Object.h"
#include "files/FileApiRouter.h"
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
        sql << "DELETE FROM shares";
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

    HttpResponse download_file(FileApiRouter& router, const std::string& file_id,
        const std::string& token) {
        HttpRequest request;
        request.set_method("GET");
        request.set_path("/files/" + file_id + "/download");
        request.add_header("Cookie", "filelink_session=" + token);

        HttpResponse response;
        router.handle_download(request, response);
        return response;
    }

    HttpResponse delete_file(FileApiRouter& router, const std::string& file_id,
        const std::string& token) {
        HttpRequest request;
        request.set_method("DELETE");
        request.set_path("/files/" + file_id);
        request.add_header("Cookie", "filelink_session=" + token);

        HttpResponse response;
        router.handle_delete(request, response);
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
    FileService file_service(pool_, ObjectStore("./storage_test"));
    ShareService share_service(pool_);
    ShareApiRouter share_api_router(server, share_service, file_service, request_authenticator);
    FileApiRouter router(server, file_service, request_authenticator, share_api_router);

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
    FileService file_service(pool_, ObjectStore("./storage_test"));
    HttpServer server("127.0.0.1", 9999);
    ShareService share_service(pool_);
    ShareApiRouter share_api_router(server, share_service, file_service, request_authenticator);
    FileApiRouter router(server, file_service, request_authenticator, share_api_router);

    const HttpResponse response = list_files(router, "");
    EXPECT_EQ(response.get_status_code(), 401);
    EXPECT_EQ(response.get_body(), R"({"message":"Unauthorized"})");
}

TEST_F(FileApiTest, DownloadsOnlyOwnersFileWithFileBody) {
    AuthService auth_service(pool_);
    AuthenticatedSession alice;
    AuthenticatedSession bob;
    ASSERT_EQ(auth_service.register_user("alice", "correct-password", alice), RegisterResult::Success);
    ASSERT_EQ(auth_service.register_user("bob", "correct-password", bob), RegisterResult::Success);

    const std::string content = "private file content";
    const std::string content_hash(32, '\0');
    const std::string file_id = "download-file000";
    ObjectStore store("./storage_test");
    ::mkdir("./storage_test", 0755);
    const std::string temporary_path = "./storage_test/file_api_download.tmp";
    {
        std::ofstream output(temporary_path, std::ios::binary);
        output << content;
    }
    const CommitResult commit = store.commit(temporary_path, std::string(64, '0'));
    {
        soci::session sql(pool_);
        db::ObjectDao(sql).add_reference(content_hash, content.size());
        db::FileDao(sql).create({file_id, alice.user_id, content_hash, "report.txt", {}});
    }

    HttpServer server("127.0.0.1", 9999);
    RequestAuthenticator request_authenticator(auth_service);
    FileService file_service(pool_, ObjectStore("./storage_test"));
    ShareService share_service(pool_);
    ShareApiRouter share_api_router(server, share_service, file_service, request_authenticator);
    FileApiRouter router(server, file_service, request_authenticator, share_api_router);

    const std::string file_id_hex = "646f776e6c6f61642d66696c65303030";
    const HttpResponse response = download_file(router, file_id_hex, alice.session_token);

    EXPECT_EQ(response.get_status_code(), 200);
    EXPECT_TRUE(response.has_file_body());
    EXPECT_EQ(response.get_file_size(), content.size());
    EXPECT_EQ(response.get_headers().at("Content-Disposition"), "attachment; filename=\"report.txt\"");

    const HttpResponse forbidden_response = download_file(router, file_id_hex, bob.session_token);
    EXPECT_EQ(forbidden_response.get_status_code(), 404);

    ::unlink(commit.objectPath.c_str());
}

TEST_F(FileApiTest, DeletesOnlyOwnersFileAndMarksLastObjectReferencePending) {
    AuthService auth_service(pool_);
    AuthenticatedSession alice;
    AuthenticatedSession bob;
    ASSERT_EQ(auth_service.register_user("alice", "correct-password", alice), RegisterResult::Success);
    ASSERT_EQ(auth_service.register_user("bob", "correct-password", bob), RegisterResult::Success);

    const std::string content_hash = "12345678901234567890123456789012";
    const std::string alice_file_id = "alice-file-id001";
    const std::string bob_file_id = "bob-file-id00001";
    {
        soci::session sql(pool_);
        db::ObjectDao(sql).add_reference(content_hash, 10);
        db::ObjectDao(sql).add_reference(content_hash, 10);
        db::FileDao(sql).create({alice_file_id, alice.user_id, content_hash, "alice.txt", {}});
        db::FileDao(sql).create({bob_file_id, bob.user_id, content_hash, "bob.txt", {}});
    }

    HttpServer server("127.0.0.1", 9999);
    RequestAuthenticator request_authenticator(auth_service);
    FileService file_service(pool_, ObjectStore("./storage_test"));
    ShareService share_service(pool_);
    ShareApiRouter share_api_router(server, share_service, file_service, request_authenticator);
    FileApiRouter router(server, file_service, request_authenticator, share_api_router);

    const std::string alice_file_id_hex = "616c6963652d66696c652d6964303031";
    const std::string bob_file_id_hex = "626f622d66696c652d69643030303031";
    EXPECT_EQ(delete_file(router, alice_file_id_hex, "").get_status_code(), 401);
    EXPECT_EQ(delete_file(router, bob_file_id_hex, alice.session_token).get_status_code(), 404);
    EXPECT_EQ(delete_file(router, alice_file_id_hex, alice.session_token).get_status_code(), 204);

    {
        soci::session sql(pool_);
        db::File file;
        EXPECT_FALSE(db::FileDao(sql).find_by_id_and_owner(alice_file_id, alice.user_id, file));

        db::Object object;
        ASSERT_TRUE(db::ObjectDao(sql).find(content_hash, object));
        EXPECT_EQ(object.ref_count, 1u);
        EXPECT_EQ(object.state, "READY");
    }

    EXPECT_EQ(delete_file(router, bob_file_id_hex, bob.session_token).get_status_code(), 204);
    {
        soci::session sql(pool_);
        db::Object object;
        ASSERT_TRUE(db::ObjectDao(sql).find(content_hash, object));
        EXPECT_EQ(object.ref_count, 0u);
        EXPECT_EQ(object.state, "PENDING_DELETE");
    }
}

} // namespace filelink
