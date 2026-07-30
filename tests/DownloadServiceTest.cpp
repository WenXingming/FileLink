// ============================================================================
// Downloads 业务集成测试：验证私有所有权和分享 Token 两种下载授权。
// HTTP 路由和响应格式分别由真实服务测试与 DownloadHttpTests 覆盖。
// ============================================================================

#include "MySqlTestConfig.h"
#include "auth/AuthService.h"
#include "database/File.h"
#include "database/Object.h"
#include "downloads/DownloadService.h"
#include "files/FileService.h"
#include "shares/ShareService.h"

#include <gtest/gtest.h>
#include <soci/mysql/soci-mysql.h>

#include <ctime>
#include <string>

namespace filelink {

class DownloadServiceTest : public testing::Test {
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

    soci::connection_pool pool_{1};
};

TEST_F(DownloadServiceTest, FindsOnlyOwnersPrivateFile) {
    AuthService auth_service(pool_);
    AuthenticatedSession alice;
    AuthenticatedSession bob;
    ASSERT_EQ(auth_service.register_user("alice", "correct-password", alice),
        RegisterResult::Success);
    ASSERT_EQ(auth_service.register_user("bob", "correct-password", bob),
        RegisterResult::Success);

    {
        soci::session sql(pool_);
        const std::string content_hash(32, '\0');
        db::ObjectDao(sql).add_reference(content_hash, 20);
        db::FileDao(sql).create(
            {"download-file000", alice.user_id, content_hash, "report.txt", {}});
    }

    FileService file_service(pool_);
    ShareService share_service(pool_);
    DownloadService download_service(file_service, share_service);
    DownloadTarget target;

    EXPECT_FALSE(download_service.find_private_download(
        bob.user_id, "download-file000", target));
    ASSERT_TRUE(download_service.find_private_download(
        alice.user_id, "download-file000", target));
    EXPECT_EQ(target.object_key, "00/00/" + std::string(64, '0'));
    EXPECT_EQ(target.display_name, "report.txt");
}

TEST_F(DownloadServiceTest, FindsFileGrantedByActiveShareToken) {
    AuthService auth_service(pool_);
    AuthenticatedSession alice;
    ASSERT_EQ(auth_service.register_user("alice", "correct-password", alice),
        RegisterResult::Success);

    {
        soci::session sql(pool_);
        const std::string content_hash(32, static_cast<char>(0xaa));
        db::ObjectDao(sql).add_reference(content_hash, 20);
        db::FileDao(sql).create(
            {"share-api-file01", alice.user_id, content_hash, "report.pdf", {}});
    }

    FileService file_service(pool_);
    ShareService share_service(pool_);
    CreatedShare share;
    ASSERT_EQ(share_service.create_share(alice.user_id, "share-api-file01",
            std::time(nullptr) + 3600, share),
        CreateShareResult::Success);

    DownloadService download_service(file_service, share_service);
    DownloadTarget target;

    ASSERT_TRUE(download_service.find_shared_download(share.token, target));
    EXPECT_EQ(target.object_key, "aa/aa/" + std::string(64, 'a'));
    EXPECT_EQ(target.display_name, "report.pdf");

    ASSERT_EQ(share_service.revoke_share(
        alice.user_id, "share-api-file01", share.share_id), RevokeShareResult::Success);
    EXPECT_FALSE(download_service.find_shared_download(share.token, target));
}

} // namespace filelink
