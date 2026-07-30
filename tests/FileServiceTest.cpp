// ============================================================================
// Files 业务集成测试：验证用户文件查询与删除事务。
// HTTP 路由行为由真实服务测试覆盖。
// ============================================================================

#include "MySqlTestConfig.h"
#include "auth/AuthService.h"
#include "database/File.h"
#include "database/Object.h"
#include "files/FileService.h"

#include <gtest/gtest.h>
#include <soci/mysql/soci-mysql.h>

#include <string>
#include <vector>

namespace filelink {

class FileServiceTest : public testing::Test {
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

TEST_F(FileServiceTest, ListsOnlyCurrentUsersFiles) {
    AuthService auth_service(pool_);
    AuthenticatedSession alice;
    AuthenticatedSession bob;
    ASSERT_EQ(auth_service.register_user("alice", "correct-password", alice),
        RegisterResult::Success);
    ASSERT_EQ(auth_service.register_user("bob", "correct-password", bob),
        RegisterResult::Success);

    const std::string content_hash = "12345678901234567890123456789012";
    {
        soci::session sql(pool_);
        db::ObjectDao(sql).add_reference(content_hash, 10);
        db::ObjectDao(sql).add_reference(content_hash, 10);
        db::FileDao(sql).create(
            {"alice-file-id001", alice.user_id, content_hash, "alice.txt", {}});
        db::FileDao(sql).create(
            {"bob-file-id00001", bob.user_id, content_hash, "bob.txt", {}});
    }

    FileService file_service(pool_);
    const std::vector<db::File> files = file_service.list_files(alice.user_id);

    ASSERT_EQ(files.size(), 1u);
    EXPECT_EQ(files[0].file_id, "alice-file-id001");
    EXPECT_EQ(files[0].display_name, "alice.txt");
}

TEST_F(FileServiceTest, DeletesOnlyOwnersFileAndMarksLastObjectReferencePending) {
    AuthService auth_service(pool_);
    AuthenticatedSession alice;
    AuthenticatedSession bob;
    ASSERT_EQ(auth_service.register_user("alice", "correct-password", alice),
        RegisterResult::Success);
    ASSERT_EQ(auth_service.register_user("bob", "correct-password", bob),
        RegisterResult::Success);

    const std::string content_hash = "12345678901234567890123456789012";
    const std::string alice_file_id = "alice-file-id001";
    const std::string bob_file_id = "bob-file-id00001";
    {
        soci::session sql(pool_);
        db::ObjectDao(sql).add_reference(content_hash, 10);
        db::ObjectDao(sql).add_reference(content_hash, 10);
        db::FileDao(sql).create(
            {alice_file_id, alice.user_id, content_hash, "alice.txt", {}});
        db::FileDao(sql).create(
            {bob_file_id, bob.user_id, content_hash, "bob.txt", {}});
    }

    FileService file_service(pool_);
    EXPECT_FALSE(file_service.delete_file(alice.user_id, bob_file_id));
    EXPECT_TRUE(file_service.delete_file(alice.user_id, alice_file_id));

    {
        soci::session sql(pool_);
        db::File file;
        EXPECT_FALSE(db::FileDao(sql).find_by_id_and_owner(
            alice_file_id, alice.user_id, file));

        db::Object object;
        ASSERT_TRUE(db::ObjectDao(sql).find(content_hash, object));
        EXPECT_EQ(object.ref_count, 1u);
        EXPECT_EQ(object.state, "READY");
    }

    EXPECT_TRUE(file_service.delete_file(bob.user_id, bob_file_id));
    {
        soci::session sql(pool_);
        db::Object object;
        ASSERT_TRUE(db::ObjectDao(sql).find(content_hash, object));
        EXPECT_EQ(object.ref_count, 0u);
        EXPECT_EQ(object.state, "PENDING_DELETE");
    }
}

} // namespace filelink
