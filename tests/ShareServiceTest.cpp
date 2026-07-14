#include "MySqlTestConfig.h"
#include "shares/ShareService.h"
#include "db/File.h"
#include "db/Object.h"
#include "db/User.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <ctime>
#include <soci/connection-pool.h>
#include <soci/mysql/soci-mysql.h>

class ShareServiceTest : public testing::Test {
protected:
    void SetUp() override {
        pool_.at(0).open(soci::mysql, filelink::test::mysql_connection_string());
        clear_database();

        soci::session sql(pool_);
        create_user(sql, "share-owner-id01", "alice");
        create_user(sql, "share-reader-id1", "bob");
        filelink::db::ObjectDao(sql).add_reference(std::string(32, 'a'), 42);
        filelink::db::FileDao(sql).create({ "share-file-id-01", "share-owner-id01",
            std::string(32, 'a'), "report.pdf", {} });
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

    void create_user(soci::session& sql, const std::string& user_id, const std::string& username) {
        filelink::db::UserDao(sql).create({ user_id, username, "$argon2id$test", false });
    }

    soci::connection_pool pool_{1};
};

TEST_F(ShareServiceTest, CreatesListsAndRevokesOwnersShare) {
    filelink::ShareService shares(pool_);
    filelink::CreatedShare created;
    const std::time_t expiry = std::time(nullptr) + 3600;

    ASSERT_EQ(shares.create_share("share-owner-id01", "share-file-id-01", expiry, created),
        filelink::CreateShareResult::Success);
    EXPECT_EQ(created.share_id.size(), 16u);
    EXPECT_EQ(created.token.size(), 64u);
    EXPECT_TRUE(std::all_of(created.token.begin(), created.token.end(),
        [](unsigned char value) { return std::isxdigit(value) != 0; }));

    {
        soci::session sql(pool_);
        std::string stored_token_hash;
        sql << "SELECT token_hash FROM shares WHERE share_id = :share_id",
            soci::into(stored_token_hash), soci::use(created.share_id);
        EXPECT_EQ(stored_token_hash.size(), 32u);
        EXPECT_NE(stored_token_hash, created.token);
    }

    std::vector<filelink::db::Share> owner_shares;
    EXPECT_TRUE(shares.list_shares("share-owner-id01", "share-file-id-01", owner_shares));
    ASSERT_EQ(owner_shares.size(), 1u);
    EXPECT_EQ(owner_shares[0].share_id, created.share_id);
    EXPECT_FALSE(shares.list_shares("share-reader-id1", "share-file-id-01", owner_shares));

    EXPECT_EQ(shares.revoke_share("share-reader-id1", "share-file-id-01", created.share_id),
        filelink::RevokeShareResult::FileNotFound);
    EXPECT_EQ(shares.revoke_share("share-owner-id01", "share-file-id-01", created.share_id),
        filelink::RevokeShareResult::Success);
    EXPECT_EQ(shares.revoke_share("share-owner-id01", "share-file-id-01", created.share_id),
        filelink::RevokeShareResult::ShareNotFound);
}

TEST_F(ShareServiceTest, RejectsPastExpiryAndFilesNotOwnedByCaller) {
    filelink::ShareService shares(pool_);
    filelink::CreatedShare created;

    EXPECT_EQ(shares.create_share("share-owner-id01", "share-file-id-01",
            std::time(nullptr) - 1, created),
        filelink::CreateShareResult::InvalidExpiry);
    EXPECT_EQ(shares.create_share("share-reader-id1", "share-file-id-01",
            std::time(nullptr) + 3600, created),
        filelink::CreateShareResult::FileNotFound);
}
