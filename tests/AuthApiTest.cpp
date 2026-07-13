#include "AuthApiRouter.h"
#include "AuthService.h"
#include "MySqlTestConfig.h"
#include "tudou/http/HttpRequest.h"
#include "tudou/http/HttpResponse.h"
#include "tudou/http/HttpServer.h"

#include <gtest/gtest.h>

#include <soci/connection-pool.h>
#include <soci/mysql/soci-mysql.h>
#include <soci/soci.h>

namespace filelink {

class AuthApiTest : public testing::Test {
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
        sql << "DELETE FROM user_sessions";
        sql << "DELETE FROM users";
    }

    HttpResponse register_user(AuthApiRouter& router, const std::string& body) {
        HttpRequest request;
        request.set_method("POST");
        request.set_path("/auth/register");
        request.set_body(body);

        HttpResponse response;
        router.handle_register(request, response);
        return response;
    }

    HttpResponse login_user(AuthApiRouter& router, const std::string& body) {
        HttpRequest request;
        request.set_method("POST");
        request.set_path("/auth/login");
        request.set_body(body);

        HttpResponse response;
        router.handle_login(request, response);
        return response;
    }

    HttpResponse current_user(AuthApiRouter& router, const std::string& cookies) {
        HttpRequest request;
        request.set_method("GET");
        request.set_path("/auth/me");
        if (!cookies.empty()) {
            request.add_header("Cookie", cookies);
        }

        HttpResponse response;
        router.handle_current_user(request, response);
        return response;
    }

    HttpResponse logout(AuthApiRouter& router, const std::string& cookies) {
        HttpRequest request;
        request.set_method("POST");
        request.set_path("/auth/logout");
        if (!cookies.empty()) {
            request.add_header("Cookie", cookies);
        }

        HttpResponse response;
        router.handle_logout(request, response);
        return response;
    }

    std::string session_token(const HttpResponse& response) {
        return response.get_headers().at("Set-Cookie").substr(17, 64);
    }

    soci::connection_pool pool_{1};
};

TEST_F(AuthApiTest, RegistersUserAndSetsSessionCookie) {
    HttpServer server("127.0.0.1", 9999);
    AuthService auth(pool_);
    AuthApiRouter router(server, auth);

    const HttpResponse response = register_user(router,
        R"({"username":"alice","password":"correct-password"})");

    EXPECT_EQ(response.get_status_code(), 201);
    EXPECT_EQ(response.get_status_message(), "Created");
    EXPECT_EQ(response.get_headers().at("Content-Type"), "application/json");
    EXPECT_EQ(response.get_body(), R"({"username":"alice"})");

    const std::string& cookie = response.get_headers().at("Set-Cookie");
    EXPECT_EQ(cookie.substr(0, 17), "filelink_session=");
    EXPECT_NE(cookie.find("; Path=/; HttpOnly; SameSite=Lax; Max-Age=604800"), std::string::npos);
}

TEST_F(AuthApiTest, RejectsInvalidBodyAndDuplicateUsername) {
    HttpServer server("127.0.0.1", 9999);
    AuthService auth(pool_);
    AuthApiRouter router(server, auth);

    EXPECT_EQ(register_user(router, R"({"username":"alice"})").get_status_code(), 400);
    EXPECT_EQ(register_user(router,
        R"({"username":"alice","password":"correct-password"})").get_status_code(), 201);
    EXPECT_EQ(register_user(router,
        R"({"username":"alice","password":"correct-password"})").get_status_code(), 409);
}

TEST_F(AuthApiTest, LogsInAndSetsNewSessionCookie) {
    HttpServer server("127.0.0.1", 9999);
    AuthService auth(pool_);
    AuthApiRouter router(server, auth);
    ASSERT_EQ(register_user(router,
        R"({"username":"alice","password":"correct-password"})").get_status_code(), 201);

    const HttpResponse response = login_user(router,
        R"({"username":"alice","password":"correct-password"})");

    EXPECT_EQ(response.get_status_code(), 200);
    EXPECT_EQ(response.get_body(), R"({"username":"alice"})");
    EXPECT_EQ(response.get_headers().at("Set-Cookie").substr(0, 17), "filelink_session=");
}

TEST_F(AuthApiTest, RejectsInvalidLoginCredentials) {
    HttpServer server("127.0.0.1", 9999);
    AuthService auth(pool_);
    AuthApiRouter router(server, auth);
    ASSERT_EQ(register_user(router,
        R"({"username":"alice","password":"correct-password"})").get_status_code(), 201);

    const HttpResponse response = login_user(router,
        R"({"username":"alice","password":"wrong-password"})");

    EXPECT_EQ(response.get_status_code(), 401);
    EXPECT_EQ(response.get_body(), R"({"message":"Invalid credentials"})");
}

TEST_F(AuthApiTest, ReturnsCurrentUserForValidSessionCookie) {
    HttpServer server("127.0.0.1", 9999);
    AuthService auth(pool_);
    AuthApiRouter router(server, auth);
    const HttpResponse registration = register_user(router,
        R"({"username":"alice","password":"correct-password"})");
    ASSERT_EQ(registration.get_status_code(), 201);

    const HttpResponse response = current_user(router,
        "theme=dark; filelink_session=" + session_token(registration));

    EXPECT_EQ(response.get_status_code(), 200);
    EXPECT_EQ(response.get_body(), R"({"username":"alice"})");
}

TEST_F(AuthApiTest, RejectsMissingMalformedAndLoggedOutSessionCookies) {
    HttpServer server("127.0.0.1", 9999);
    AuthService auth(pool_);
    AuthApiRouter router(server, auth);
    const HttpResponse registration = register_user(router,
        R"({"username":"alice","password":"correct-password"})");
    ASSERT_EQ(registration.get_status_code(), 201);

    EXPECT_EQ(current_user(router, "").get_status_code(), 401);
    EXPECT_EQ(current_user(router, "filelink_session=not-a-token").get_status_code(), 401);

    const std::string token = session_token(registration);
    ASSERT_EQ(auth.logout(token), LogoutResult::Success);
    EXPECT_EQ(current_user(router, "filelink_session=" + token).get_status_code(), 401);
}

TEST_F(AuthApiTest, LogsOutAndClearsSessionCookie) {
    HttpServer server("127.0.0.1", 9999);
    AuthService auth(pool_);
    AuthApiRouter router(server, auth);
    const HttpResponse registration = register_user(router,
        R"({"username":"alice","password":"correct-password"})");
    ASSERT_EQ(registration.get_status_code(), 201);

    const std::string token = session_token(registration);
    const HttpResponse response = logout(router, "filelink_session=" + token);

    EXPECT_EQ(response.get_status_code(), 204);
    EXPECT_EQ(response.get_headers().at("Set-Cookie"),
        "filelink_session=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0");
    EXPECT_EQ(current_user(router, "filelink_session=" + token).get_status_code(), 401);
}

TEST_F(AuthApiTest, LogoutWithoutSessionIsIdempotent) {
    HttpServer server("127.0.0.1", 9999);
    AuthService auth(pool_);
    AuthApiRouter router(server, auth);

    const HttpResponse response = logout(router, "");

    EXPECT_EQ(response.get_status_code(), 204);
    EXPECT_EQ(response.get_headers().at("Set-Cookie"),
        "filelink_session=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0");
}

} // namespace filelink
