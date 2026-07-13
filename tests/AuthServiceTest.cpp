#include "AuthService.h"
#include "MySqlTestConfig.h"
#include "PasswordHasher.h"
#include "db/User.h"

#include <gtest/gtest.h>

#include <soci/connection-pool.h>
#include <soci/mysql/soci-mysql.h>

#include <algorithm>
#include <cctype>

class AuthServiceTest : public testing::Test {
protected:
    void SetUp() override {
        pool_.at(0).open(soci::mysql, filelink::test::mysql_connection_string());
        clear_database();
    }

    void TearDown() override {
        clear_database();
    }

    void clear_database() {
        soci::session sql(pool_);
        sql << "DELETE FROM user_sessions";
        sql << "DELETE FROM users";
    }

    soci::connection_pool pool_{1};
};

TEST_F(AuthServiceTest, RegistersUserAndCreatesSession) {
    filelink::AuthService auth(pool_);
    filelink::AuthenticatedSession registration;

    ASSERT_EQ(auth.register_user("alice", "correct-password", registration),
        filelink::RegisterResult::Success);
    EXPECT_EQ(registration.username, "alice");
    EXPECT_EQ(registration.user_id.size(), 16U);
    EXPECT_EQ(registration.session_token.size(), 64U);
    EXPECT_TRUE(std::all_of(registration.session_token.begin(), registration.session_token.end(),
        [](unsigned char value) { return std::isxdigit(value) != 0; }));

    soci::session sql(pool_);
    filelink::db::User user;
    ASSERT_TRUE(filelink::db::UserDao(sql).find_by_id(registration.user_id, user));
    EXPECT_TRUE(filelink::PasswordHasher::verify("correct-password", user.password_hash));

    int session_count = 0;
    sql << "SELECT COUNT(*) FROM user_sessions WHERE user_id = :id",
        soci::into(session_count), soci::use(registration.user_id);
    EXPECT_EQ(session_count, 1);
}

TEST_F(AuthServiceTest, RejectsInvalidAndDuplicateRegistration) {
    filelink::AuthService auth(pool_);
    filelink::AuthenticatedSession registration;

    EXPECT_EQ(auth.register_user("no space", "correct-password", registration),
        filelink::RegisterResult::InvalidUsername);
    EXPECT_EQ(auth.register_user("alice", "short", registration),
        filelink::RegisterResult::InvalidPassword);
    ASSERT_EQ(auth.register_user("alice", "correct-password", registration),
        filelink::RegisterResult::Success);
    EXPECT_EQ(auth.register_user("alice", "another-password", registration),
        filelink::RegisterResult::UsernameTaken);
}

TEST_F(AuthServiceTest, LogsInAndCreatesAnotherSession) {
    filelink::AuthService auth(pool_);
    filelink::AuthenticatedSession registration;
    ASSERT_EQ(auth.register_user("alice", "correct-password", registration),
        filelink::RegisterResult::Success);

    filelink::AuthenticatedSession login;
    ASSERT_EQ(auth.login_user("alice", "correct-password", login),
        filelink::LoginResult::Success);
    EXPECT_EQ(login.user_id, registration.user_id);
    EXPECT_NE(login.session_token, registration.session_token);

    soci::session sql(pool_);
    int session_count = 0;
    sql << "SELECT COUNT(*) FROM user_sessions WHERE user_id = :id",
        soci::into(session_count), soci::use(login.user_id);
    EXPECT_EQ(session_count, 2);
}

TEST_F(AuthServiceTest, RejectsUnknownWrongPasswordAndDisabledUser) {
    filelink::AuthService auth(pool_);
    filelink::AuthenticatedSession session;

    EXPECT_EQ(auth.login_user("missing", "correct-password", session),
        filelink::LoginResult::InvalidCredentials);
    ASSERT_EQ(auth.register_user("alice", "correct-password", session),
        filelink::RegisterResult::Success);
    EXPECT_EQ(auth.login_user("alice", "wrong-password", session),
        filelink::LoginResult::InvalidCredentials);

    {
        soci::session sql(pool_);
        sql << "UPDATE users SET disabled_at = NOW() WHERE username = 'alice'";
    }
    EXPECT_EQ(auth.login_user("alice", "correct-password", session),
        filelink::LoginResult::InvalidCredentials);
}
