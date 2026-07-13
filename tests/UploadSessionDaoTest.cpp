#include <gtest/gtest.h>
#include "MySqlTestConfig.h"
#include "db/UploadSession.h"
#include "db/User.h"
#include "cleaner/SessionCleaner.h"
#include <soci/soci.h>
#include <soci/mysql/soci-mysql.h>
#include <fstream>
#include <sys/stat.h>

#include <exception>
#include <string>

using namespace filelink;
using namespace filelink::db;

class UploadSessionDaoTest : public ::testing::Test {
protected:
    void SetUp() override {
        try {
            sql.open(soci::mysql, filelink::test::mysql_connection_string());
            sql << "DELETE FROM upload_sessions";
            sql << "DELETE FROM user_sessions";
            sql << "DELETE FROM users";

            User owner;
            owner.user_id = owner_user_id_;
            owner.username = "upload_owner";
            owner.password_hash = "test-hash";
            UserDao(sql).create(owner);
        }
        catch (const std::exception& error) {
            FAIL() << "MySQL 集成测试初始化失败: " << error.what();
        }
    }

    void TearDown() override {
        if (!sql.is_connected()) {
            return;
        }
        try {
            sql << "DELETE FROM upload_sessions";
            sql << "DELETE FROM user_sessions";
            sql << "DELETE FROM users";
            sql.close();
        }
        catch (const std::exception& error) {
            ADD_FAILURE() << "MySQL 集成测试清理失败: " << error.what();
        }
    }

    soci::session sql;
    const std::string owner_user_id_ = "upload-owner-id1";
};

TEST_F(UploadSessionDaoTest, CreateAndFindSession) {
    UploadSessionDao store(sql);

    UploadSession session;
    session.upload_id = "1234567890123456"; // 16 bytes for BINARY(16)
    session.owner_user_id = owner_user_id_;
    session.state = "UPLOADING";
    session.file_name = "test_dataset.tar.gz";
    session.total_size = 10485760; // 10MB
    session.committed_offset = 0;
    session.has_expected_hash = true;
    session.expected_hash = "abcdef1234567890abcdef1234567890"; // 32 bytes for BINARY(32)
    session.has_content_hash = false;
    session.has_failure_reason = false;

    // Use current time for expires_at
    std::time_t t = std::time(nullptr);
    std::tm* tm_ptr = std::localtime(&t);
    session.expires_at = *tm_ptr;

    // Create
    ASSERT_NO_THROW(store.create(session));

    // Find
    UploadSession found;
    bool exists = store.find("1234567890123456", found);
    ASSERT_TRUE(exists);
    EXPECT_EQ(found.owner_user_id, owner_user_id_);
    EXPECT_EQ(found.state, "UPLOADING");
    EXPECT_EQ(found.file_name, "test_dataset.tar.gz");
    EXPECT_EQ(found.total_size, 10485760);
    EXPECT_EQ(found.committed_offset, 0);

    ASSERT_TRUE(found.has_expected_hash);
    EXPECT_EQ(found.expected_hash, "abcdef1234567890abcdef1234567890");

    EXPECT_FALSE(found.has_content_hash);
    EXPECT_FALSE(found.has_failure_reason);
}

TEST_F(UploadSessionDaoTest, CannotCreateWithOffsetGreaterThanTotalSize) {
    UploadSessionDao store(sql);

    UploadSession session;
    session.upload_id = "abcdefghijklmnop";
    session.owner_user_id = owner_user_id_;
    session.state = "UPLOADING";
    session.file_name = "test2.bin";
    session.total_size = 100;
    session.committed_offset = 200; // Violates CHECK constraint

    std::time_t t = std::time(nullptr);
    std::tm* tm_ptr = std::localtime(&t);
    session.expires_at = *tm_ptr;

    EXPECT_THROW(store.create(session), soci::soci_error);
}

TEST_F(UploadSessionDaoTest, SessionCleanerCleansExpiredSessionAndFiles) {
    UploadSessionDao store(sql);

    // 1. Create expired session
    UploadSession session;
    session.upload_id = "1111222233334444"; // 16 bytes
    session.owner_user_id = owner_user_id_;
    session.state = "UPLOADING";
    session.file_name = "expired.bin";
    session.total_size = 100;
    session.committed_offset = 0;
    session.has_expected_hash = false;
    session.has_content_hash = false;
    session.has_failure_reason = false;

    std::time_t t = std::time(nullptr) - 10; // 10 seconds ago
    session.expires_at = *std::localtime(&t);
    store.create(session);

    // 2. Create local part file
    std::string testStorage = "./storage_test";
    std::string uploadsDir = testStorage + "/uploads";
    ::mkdir(testStorage.c_str(), 0755);
    ::mkdir(uploadsDir.c_str(), 0755);
    
    // "1111222233334444" in hex is "31313131323232323333333334343434"
    std::string partPath = uploadsDir + "/31313131323232323333333334343434.part";
    std::ofstream ofs(partPath);
    ofs << "expired_data";
    ofs.close();

    struct stat st;
    ASSERT_EQ(::stat(partPath.c_str(), &st), 0);

    // 3. Perform cleanup
    soci::connection_pool pool(1);
    pool.at(0).open(soci::mysql, filelink::test::mysql_connection_string());
    
    SessionCleaner cleaner(pool, testStorage);
    int cleaned = cleaner.cleanup_expired_sessions();
    EXPECT_EQ(cleaned, 1);

    // 4. Verify physical file deleted
    EXPECT_NE(::stat(partPath.c_str(), &st), 0);

    // 5. Verify database state updated to EXPIRED
    UploadSession found;
    bool exists = store.find("1111222233334444", found);
    ASSERT_TRUE(exists);
    EXPECT_EQ(found.state, "EXPIRED");
}
