#include "MySqlTestConfig.h"
#include "db/User.h"
#include "db/UserSession.h"

#include <gtest/gtest.h>

#include <soci/mysql/soci-mysql.h>

#include <ctime>

class UserSessionDaoTest : public testing::Test {
protected:
    void SetUp() override {
        sql_.open(soci::mysql, filelink::test::mysql_connection_string());
        sql_ << "DELETE FROM user_sessions";
        sql_ << "DELETE FROM users";

        filelink::db::User user;
        user.user_id = "1234567890123456";
        user.username = "alice";
        user.password_hash = "$argon2id$example";
        filelink::db::UserDao(sql_).create(user);
    }

    void TearDown() override {
        if (sql_.is_connected()) {
            sql_ << "DELETE FROM user_sessions";
            sql_ << "DELETE FROM users";
        }
    }

    static std::tm time_after_seconds(int seconds) {
        const std::time_t value = std::time(nullptr) + seconds;
        return *std::localtime(&value);
    }

    soci::session sql_;
};

TEST_F(UserSessionDaoTest, CreatesFindsAndRemovesActiveSession) {
    filelink::db::UserSessionDao sessions(sql_);
    filelink::db::UserSession session;
    session.token_hash = "12345678901234567890123456789012";
    session.user_id = "1234567890123456";
    session.expires_at = time_after_seconds(3600);
    sessions.create(session);

    filelink::db::UserSession found;
    ASSERT_TRUE(sessions.find_active(session.token_hash, found));
    EXPECT_EQ(found.user_id, session.user_id);

    sessions.remove(session.token_hash);
    EXPECT_FALSE(sessions.find_active(session.token_hash, found));
}

TEST_F(UserSessionDaoTest, DoesNotFindExpiredSession) {
    filelink::db::UserSessionDao sessions(sql_);
    filelink::db::UserSession session;
    session.token_hash = "abcdefghijklmnopqrstuvwx12345678";
    session.user_id = "1234567890123456";
    session.expires_at = time_after_seconds(-60);
    sessions.create(session);

    filelink::db::UserSession found;
    EXPECT_FALSE(sessions.find_active(session.token_hash, found));
}
