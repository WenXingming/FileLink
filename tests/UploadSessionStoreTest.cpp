#include <gtest/gtest.h>
#include "store/UploadSessionStore.h"
#include <soci/soci.h>
#include <soci/mysql/soci-mysql.h>
#include <string>

using namespace filelink;
using namespace filelink::store;
using namespace filelink::models;

class UploadSessionStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Connect to the test database (using hardcoded credentials for this test)
        try {
            sql.open(soci::mysql, "db=filelink user=filelink password=12345678 host=127.0.0.1 port=3307");
            
            // Clean up the table before tests
            sql << "DELETE FROM upload_sessions";
        } catch (const soci::soci_error& e) {
            std::cerr << "SOCI Error during setup: " << e.what() << std::endl;
            // It's possible the database is not accessible during some test environments
            // If the connection fails, tests will likely fail, but we'll print it here.
        }
    }
    
    void TearDown() override {
        try {
            sql << "DELETE FROM upload_sessions";
            sql.close();
        } catch (...) {}
    }

    soci::session sql;
};

TEST_F(UploadSessionStoreTest, CreateAndFindSession) {
    UploadSessionStore store(sql);

    UploadSession session;
    session.upload_id = "1234567890123456"; // 16 bytes for BINARY(16)
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
    EXPECT_EQ(found.state, "UPLOADING");
    EXPECT_EQ(found.file_name, "test_dataset.tar.gz");
    EXPECT_EQ(found.total_size, 10485760);
    EXPECT_EQ(found.committed_offset, 0);
    
    ASSERT_TRUE(found.has_expected_hash);
    EXPECT_EQ(found.expected_hash, "abcdef1234567890abcdef1234567890");
    
    EXPECT_FALSE(found.has_content_hash);
    EXPECT_FALSE(found.has_failure_reason);
}

TEST_F(UploadSessionStoreTest, CannotCreateWithOffsetGreaterThanTotalSize) {
    UploadSessionStore store(sql);

    UploadSession session;
    session.upload_id = "abcdefghijklmnop"; 
    session.state = "UPLOADING";
    session.file_name = "test2.bin";
    session.total_size = 100;
    session.committed_offset = 200; // Violates CHECK constraint
    
    std::time_t t = std::time(nullptr);
    std::tm* tm_ptr = std::localtime(&t);
    session.expires_at = *tm_ptr;

    EXPECT_THROW(store.create(session), soci::soci_error);
}
