#include "MySqlTestConfig.h"
#include "database/User.h"

#include <gtest/gtest.h>

#include <soci/mysql/soci-mysql.h>

class UserDaoTest : public testing::Test {
protected:
    void SetUp() override {
        sql_.open(soci::mysql, filelink::test::mysql_connection_string());
        sql_ << "DELETE FROM user_sessions";
        sql_ << "DELETE FROM users";
    }

    void TearDown() override {
        if (sql_.is_connected()) {
            sql_ << "DELETE FROM user_sessions";
            sql_ << "DELETE FROM users";
        }
    }

    soci::session sql_;
};

TEST_F(UserDaoTest, CreatesAndFindsUser) {
    filelink::db::UserDao users(sql_);
    filelink::db::User user;
    user.user_id = "1234567890123456";
    user.username = "alice";
    user.password_hash = "$argon2id$example";

    users.create(user);

    filelink::db::User by_id;
    ASSERT_TRUE(users.find_by_id(user.user_id, by_id));
    EXPECT_EQ(by_id.username, "alice");
    EXPECT_EQ(by_id.password_hash, "$argon2id$example");
    EXPECT_FALSE(by_id.is_disabled);

    filelink::db::User by_username;
    ASSERT_TRUE(users.find_by_username("alice", by_username));
    EXPECT_EQ(by_username.user_id, user.user_id);
    EXPECT_FALSE(users.find_by_username("missing", by_username));
}

TEST_F(UserDaoTest, RejectsDuplicateUsername) {
    filelink::db::UserDao users(sql_);
    filelink::db::User first;
    first.user_id = "1234567890123456";
    first.username = "alice";
    first.password_hash = "$argon2id$first";
    users.create(first);

    filelink::db::User duplicate;
    duplicate.user_id = "abcdefghijklmnop";
    duplicate.username = "alice";
    duplicate.password_hash = "$argon2id$second";

    EXPECT_THROW(users.create(duplicate), soci::soci_error);
}
