#include "MySqlTestConfig.h"
#include "database/Object.h"

#include <gtest/gtest.h>

#include <soci/mysql/soci-mysql.h>

class ObjectDaoTest : public testing::Test {
protected:
    void SetUp() override {
        sql_.open(soci::mysql, filelink::test::mysql_connection_string());
        clear_objects();
    }

    void TearDown() override {
        if (sql_.is_connected()) {
            clear_objects();
        }
    }

    void clear_objects() {
        sql_ << "DELETE FROM upload_sessions";
        sql_ << "DELETE FROM shares";
        sql_ << "DELETE FROM files";
        sql_ << "DELETE FROM objects";
    }

    soci::session sql_;
};

TEST_F(ObjectDaoTest, AddsReferencesAndFindsObject) {
    filelink::db::ObjectDao objects(sql_);
    const std::string content_hash = "12345678901234567890123456789012";

    EXPECT_EQ(objects.add_reference(content_hash, 42), filelink::db::ObjectReferenceResult::Referenced);
    EXPECT_EQ(objects.add_reference(content_hash, 42), filelink::db::ObjectReferenceResult::Referenced);

    filelink::db::Object found;
    ASSERT_TRUE(objects.find(content_hash, found));
    EXPECT_EQ(found.content_hash, content_hash);
    EXPECT_EQ(found.byte_size, 42u);
    EXPECT_EQ(found.ref_count, 2u);
    EXPECT_EQ(found.state, "READY");
    EXPECT_FALSE(objects.find("abcdefghijklmnopqrstuvwxzy123456", found));
}

TEST_F(ObjectDaoTest, AddsReferenceOnlyToMatchingExistingObject) {
    filelink::db::ObjectDao objects(sql_);
    const std::string content_hash = "12345678901234567890123456789012";
    const std::string missing_hash = "abcdefghijklmnopqrstuvwxzy123456";

    filelink::db::Object missing;
    EXPECT_FALSE(objects.try_add_existing_reference(missing_hash, 42));
    EXPECT_FALSE(objects.find(missing_hash, missing));

    ASSERT_EQ(objects.add_reference(content_hash, 42),
        filelink::db::ObjectReferenceResult::Referenced);
    EXPECT_FALSE(objects.try_add_existing_reference(content_hash, 43));
    EXPECT_TRUE(objects.try_add_existing_reference(content_hash, 42));

    filelink::db::Object found;
    ASSERT_TRUE(objects.find(content_hash, found));
    EXPECT_EQ(found.ref_count, 2u);
    EXPECT_EQ(found.state, "READY");
}

TEST_F(ObjectDaoTest, RevivesPendingObjectButNotReclaimingObject) {
    filelink::db::ObjectDao objects(sql_);
    const std::string content_hash = "12345678901234567890123456789012";

    ASSERT_EQ(objects.add_reference(content_hash, 42),
        filelink::db::ObjectReferenceResult::Referenced);
    ASSERT_TRUE(objects.remove_reference(content_hash));
    ASSERT_TRUE(objects.try_add_existing_reference(content_hash, 42));

    filelink::db::Object found;
    ASSERT_TRUE(objects.find(content_hash, found));
    EXPECT_EQ(found.ref_count, 1u);
    EXPECT_EQ(found.state, "READY");

    ASSERT_TRUE(objects.remove_reference(content_hash));
    ASSERT_TRUE(objects.claim_pending_delete(content_hash));
    EXPECT_FALSE(objects.try_add_existing_reference(content_hash, 42));

    ASSERT_TRUE(objects.find(content_hash, found));
    EXPECT_EQ(found.ref_count, 0u);
    EXPECT_EQ(found.state, "RECLAIMING");
}

TEST_F(ObjectDaoTest, ClaimsAndReleasesPendingDeletionObject) {
    filelink::db::ObjectDao objects(sql_);
    const std::string content_hash = "12345678901234567890123456789012";

    EXPECT_EQ(objects.add_reference(content_hash, 42), filelink::db::ObjectReferenceResult::Referenced);
    ASSERT_TRUE(objects.remove_reference(content_hash));

    EXPECT_TRUE(objects.claim_pending_delete(content_hash));
    EXPECT_FALSE(objects.claim_pending_delete(content_hash));

    filelink::db::Object found;
    ASSERT_TRUE(objects.find(content_hash, found));
    EXPECT_EQ(found.ref_count, 0u);
    EXPECT_EQ(found.state, "RECLAIMING");

    EXPECT_TRUE(objects.return_to_pending_delete(content_hash));
    ASSERT_TRUE(objects.find(content_hash, found));
    EXPECT_EQ(found.state, "PENDING_DELETE");
}

TEST_F(ObjectDaoTest, DoesNotReviveObjectClaimedByReclaimer) {
    filelink::db::ObjectDao objects(sql_);
    const std::string content_hash = "12345678901234567890123456789012";

    EXPECT_EQ(objects.add_reference(content_hash, 42), filelink::db::ObjectReferenceResult::Referenced);
    ASSERT_TRUE(objects.remove_reference(content_hash));
    ASSERT_TRUE(objects.claim_pending_delete(content_hash));

    EXPECT_EQ(objects.add_reference(content_hash, 42), filelink::db::ObjectReferenceResult::Reclaiming);

    filelink::db::Object found;
    ASSERT_TRUE(objects.find(content_hash, found));
    EXPECT_EQ(found.ref_count, 0u);
    EXPECT_EQ(found.state, "RECLAIMING");
}
