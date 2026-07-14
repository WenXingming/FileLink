#include "auth/AuthService.h"
#include "MySqlTestConfig.h"
#include "auth/PasswordHasher.h"
#include "cache/RedisSessionCache.h"
#include "database/User.h"

#include <gtest/gtest.h>

#include <soci/connection-pool.h>
#include <soci/mysql/soci-mysql.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>

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
        sql << "DELETE FROM upload_sessions";
        sql << "DELETE FROM shares";
        sql << "DELETE FROM files";
        sql << "DELETE FROM objects";
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

TEST_F(AuthServiceTest, FindsCurrentUserFromActiveSession) {
    filelink::AuthService auth(pool_);
    filelink::AuthenticatedSession session;
    ASSERT_EQ(auth.register_user("alice", "correct-password", session),
        filelink::RegisterResult::Success);

    filelink::AuthenticatedUser user;
    ASSERT_EQ(auth.current_user(session.session_token, user),
        filelink::CurrentUserResult::Success);
    EXPECT_EQ(user.user_id, session.user_id);
    EXPECT_EQ(user.username, "alice");
}

TEST_F(AuthServiceTest, FallsBackToMySqlWhenRedisCacheIsDisabled) {
    filelink::cache::RedisSessionCache cache({});
    filelink::AuthService auth(pool_, &cache);
    filelink::AuthenticatedSession session;
    ASSERT_EQ(auth.register_user("alice", "correct-password", session),
        filelink::RegisterResult::Success);

    filelink::AuthenticatedUser user;
    EXPECT_EQ(auth.current_user(session.session_token, user), filelink::CurrentUserResult::Success);
    EXPECT_EQ(user.username, "alice");
}

TEST_F(AuthServiceTest, RejectsMalformedExpiredAndDisabledSessions) {
    filelink::AuthService auth(pool_);
    filelink::AuthenticatedSession expired_session;
    ASSERT_EQ(auth.register_user("alice", "correct-password", expired_session),
        filelink::RegisterResult::Success);

    filelink::AuthenticatedUser user;
    EXPECT_EQ(auth.current_user("not-a-token", user), filelink::CurrentUserResult::InvalidSession);
    {
        soci::session sql(pool_);
        sql << "UPDATE user_sessions SET expires_at = DATE_SUB(NOW(), INTERVAL 1 SECOND) "
               "WHERE user_id = :id",
            soci::use(expired_session.user_id);
    }
    EXPECT_EQ(auth.current_user(expired_session.session_token, user),
        filelink::CurrentUserResult::InvalidSession);

    filelink::AuthenticatedSession disabled_session;
    ASSERT_EQ(auth.register_user("bob", "correct-password", disabled_session),
        filelink::RegisterResult::Success);
    {
        soci::session sql(pool_);
        sql << "UPDATE users SET disabled_at = NOW() WHERE user_id = :id",
            soci::use(disabled_session.user_id);
    }
    EXPECT_EQ(auth.current_user(disabled_session.session_token, user),
        filelink::CurrentUserResult::InvalidSession);
}

TEST_F(AuthServiceTest, LogsOutByDeletingOnlyTheSpecifiedSession) {
    filelink::AuthService auth(pool_);
    filelink::AuthenticatedSession registration;
    ASSERT_EQ(auth.register_user("alice", "correct-password", registration),
        filelink::RegisterResult::Success);

    filelink::AuthenticatedSession login;
    ASSERT_EQ(auth.login_user("alice", "correct-password", login),
        filelink::LoginResult::Success);

    ASSERT_EQ(auth.logout(login.session_token), filelink::LogoutResult::Success);
    filelink::AuthenticatedUser user;
    EXPECT_EQ(auth.current_user(login.session_token, user),
        filelink::CurrentUserResult::InvalidSession);
    EXPECT_EQ(auth.current_user(registration.session_token, user),
        filelink::CurrentUserResult::Success);
}

TEST_F(AuthServiceTest, LogoutIsIdempotentAndRejectsMalformedToken) {
    filelink::AuthService auth(pool_);
    filelink::AuthenticatedSession session;
    ASSERT_EQ(auth.register_user("alice", "correct-password", session),
        filelink::RegisterResult::Success);

    EXPECT_EQ(auth.logout(session.session_token), filelink::LogoutResult::Success);
    EXPECT_EQ(auth.logout(session.session_token), filelink::LogoutResult::Success);
    EXPECT_EQ(auth.logout("not-a-token"), filelink::LogoutResult::InvalidSession);
}

TEST_F(AuthServiceTest, RedisCacheDoesNotKeepLoggedOutSessionAuthenticated) {
    const char* redis_port = std::getenv("FILELINK_TEST_REDIS_PORT");
    if (redis_port == nullptr || redis_port[0] == '\0') {
        GTEST_SKIP() << "set FILELINK_TEST_REDIS_PORT to run Redis integration coverage";
    }

    filelink::cache::RedisConfig config;
    config.enabled = true;
    config.port = static_cast<uint16_t>(std::strtoul(redis_port, nullptr, 10));
    filelink::cache::RedisSessionCache cache(config);
    filelink::AuthService auth(pool_, &cache);
    filelink::AuthenticatedSession session;
    ASSERT_EQ(auth.register_user("alice", "correct-password", session),
        filelink::RegisterResult::Success);

    filelink::AuthenticatedUser user;
    ASSERT_EQ(auth.current_user(session.session_token, user), filelink::CurrentUserResult::Success);
    ASSERT_EQ(auth.logout(session.session_token), filelink::LogoutResult::Success);
    EXPECT_EQ(auth.current_user(session.session_token, user), filelink::CurrentUserResult::InvalidSession);
}
