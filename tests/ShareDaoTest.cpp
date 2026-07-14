#include "MySqlTestConfig.h"
#include "db/File.h"
#include "db/Object.h"
#include "db/Share.h"
#include "db/User.h"

#include <gtest/gtest.h>

#include <ctime>
#include <soci/mysql/soci-mysql.h>

class ShareDaoTest : public testing::Test {
protected:
    void SetUp() override {
        sql_.open(soci::mysql, filelink::test::mysql_connection_string());
        clear_database();

        const std::string user_id = "share-owner-id01";
        const std::string content_hash(32, 'a');
        filelink::db::UserDao(sql_).create({ user_id, "share-owner", "$argon2id$test", false });
        filelink::db::ObjectDao(sql_).add_reference(content_hash, 42);
        filelink::db::FileDao(sql_).create({ "share-file-id-01", user_id, content_hash,
            "report.pdf", {} });
    }

    void TearDown() override {
        if (sql_.is_connected()) {
            clear_database();
        }
    }

    void clear_database() {
        sql_ << "DELETE FROM upload_sessions";
        sql_ << "DELETE FROM shares";
        sql_ << "DELETE FROM files";
        sql_ << "DELETE FROM objects";
        sql_ << "DELETE FROM user_sessions";
        sql_ << "DELETE FROM users";
    }

    std::tm time_after(int seconds) const {
        const std::time_t value = std::time(nullptr) + seconds;
        return *std::localtime(&value);
    }

    soci::session sql_;
};

TEST_F(ShareDaoTest, CreatesFindsRevokesAndCascadesShares) {
    const std::string file_id = "share-file-id-01";
    const std::string active_hash(32, 'b');
    const std::string expired_hash(32, 'c');
    filelink::db::ShareDao shares(sql_);

    shares.create({ "active-share-001", file_id, active_hash, time_after(3600), {} });
    shares.create({ "expired-share001", file_id, expired_hash, time_after(-3600), {} });

    std::vector<filelink::db::Share> active_shares;
    shares.find_active_by_file_id(file_id, active_shares);
    ASSERT_EQ(active_shares.size(), 1u);
    EXPECT_EQ(active_shares[0].share_id, "active-share-001");

    filelink::db::Share found_share;
    EXPECT_TRUE(shares.find_active_by_token_hash(active_hash, found_share));
    EXPECT_EQ(found_share.file_id, file_id);
    EXPECT_FALSE(shares.find_active_by_token_hash(expired_hash, found_share));

    EXPECT_TRUE(shares.revoke_by_id_and_file_id("active-share-001", file_id));
    EXPECT_FALSE(shares.revoke_by_id_and_file_id("active-share-001", file_id));
    EXPECT_FALSE(shares.find_active_by_token_hash(active_hash, found_share));

    const std::string cascade_hash(32, 'd');
    shares.create({ "cascade-share-id", file_id, cascade_hash, time_after(3600), {} });
    EXPECT_TRUE(filelink::db::FileDao(sql_).remove_by_id_and_owner(file_id, "share-owner-id01"));
    EXPECT_FALSE(shares.find_active_by_token_hash(cascade_hash, found_share));
}
