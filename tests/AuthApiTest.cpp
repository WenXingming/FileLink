#include "auth/AuthApiRouter.h"
#include "auth/AuthService.h"
#include "MySqlTestConfig.h"
#include "tudou/http/HttpRequest.h"
#include "tudou/http/HttpResponse.h"
#include "tudou/http/HttpServer.h"

#include <gtest/gtest.h>

#include <soci/connection-pool.h>
#include <soci/mysql/soci-mysql.h>
#include <soci/soci.h>

namespace filelink {

namespace {

struct HttpServerTestInspector {
    std::string ip;
    uint16_t port;
    TcpServer tcpServer;
    std::unordered_map<TcpConnection*, std::shared_ptr<HttpConnection>> httpConnections;
    std::mutex contextsMutex;
    HttpRouter router;
};

inline const HttpRouter& get_http_router(const HttpServer& server) {
    return reinterpret_cast<const HttpServerTestInspector&>(server).router;
}

} // namespace

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
        sql << "DELETE FROM upload_sessions";
        sql << "DELETE FROM shares";
        sql << "DELETE FROM files";
        sql << "DELETE FROM objects";
        sql << "DELETE FROM user_sessions";
        sql << "DELETE FROM users";
    }

    HttpResponse dispatch(HttpServer& server, const HttpRequest& request) {
        HttpResponse response;
        get_http_router(server).dispatch(request, response);
        return response;
    }

    HttpResponse register_user(HttpServer& server, const std::string& body) {
        HttpRequest request;
        request.set_method("POST");
        request.set_path("/auth/register");
        request.set_body(body);

        return dispatch(server, request);
    }

    HttpResponse login_user(HttpServer& server, const std::string& body) {
        HttpRequest request;
        request.set_method("POST");
        request.set_path("/auth/login");
        request.set_body(body);

        return dispatch(server, request);
    }

    HttpResponse current_user(HttpServer& server, const std::string& cookies) {
        HttpRequest request;
        request.set_method("GET");
        request.set_path("/auth/me");
        if (!cookies.empty()) {
            request.add_header("Cookie", cookies);
        }

        return dispatch(server, request);
    }

    HttpResponse logout(HttpServer& server, const std::string& cookies) {
        HttpRequest request;
        request.set_method("POST");
        request.set_path("/auth/logout");
        if (!cookies.empty()) {
            request.add_header("Cookie", cookies);
        }

        return dispatch(server, request);
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
    router.register_routes();

    const HttpResponse response = register_user(server,
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
    router.register_routes();

    EXPECT_EQ(register_user(server, R"({"username":"alice"})").get_status_code(), 400);
    EXPECT_EQ(register_user(server,
        R"({"username":"alice","password":"correct-password"})").get_status_code(), 201);
    EXPECT_EQ(register_user(server,
        R"({"username":"alice","password":"correct-password"})").get_status_code(), 409);
}

TEST_F(AuthApiTest, LogsInAndSetsNewSessionCookie) {
    HttpServer server("127.0.0.1", 9999);
    AuthService auth(pool_);
    AuthApiRouter router(server, auth);
    router.register_routes();
    ASSERT_EQ(register_user(server,
        R"({"username":"alice","password":"correct-password"})").get_status_code(), 201);

    const HttpResponse response = login_user(server,
        R"({"username":"alice","password":"correct-password"})");

    EXPECT_EQ(response.get_status_code(), 200);
    EXPECT_EQ(response.get_body(), R"({"username":"alice"})");
    EXPECT_EQ(response.get_headers().at("Set-Cookie").substr(0, 17), "filelink_session=");
}

TEST_F(AuthApiTest, RejectsInvalidLoginCredentials) {
    HttpServer server("127.0.0.1", 9999);
    AuthService auth(pool_);
    AuthApiRouter router(server, auth);
    router.register_routes();
    ASSERT_EQ(register_user(server,
        R"({"username":"alice","password":"correct-password"})").get_status_code(), 201);

    const HttpResponse response = login_user(server,
        R"({"username":"alice","password":"wrong-password"})");

    EXPECT_EQ(response.get_status_code(), 401);
    EXPECT_EQ(response.get_body(), R"({"message":"Invalid credentials"})");
}

TEST_F(AuthApiTest, ReturnsCurrentUserForValidSessionCookie) {
    HttpServer server("127.0.0.1", 9999);
    AuthService auth(pool_);
    AuthApiRouter router(server, auth);
    router.register_routes();
    const HttpResponse registration = register_user(server,
        R"({"username":"alice","password":"correct-password"})");
    ASSERT_EQ(registration.get_status_code(), 201);

    const HttpResponse response = current_user(server,
        "theme=dark; filelink_session=" + session_token(registration));

    EXPECT_EQ(response.get_status_code(), 200);
    EXPECT_EQ(response.get_body(), R"({"username":"alice"})");
}

TEST_F(AuthApiTest, RejectsMissingMalformedAndLoggedOutSessionCookies) {
    HttpServer server("127.0.0.1", 9999);
    AuthService auth(pool_);
    AuthApiRouter router(server, auth);
    router.register_routes();
    const HttpResponse registration = register_user(server,
        R"({"username":"alice","password":"correct-password"})");
    ASSERT_EQ(registration.get_status_code(), 201);

    EXPECT_EQ(current_user(server, "").get_status_code(), 401);
    EXPECT_EQ(current_user(server, "filelink_session=not-a-token").get_status_code(), 401);

    const std::string token = session_token(registration);
    EXPECT_EQ(current_user(server,
        "filelink_session=" + token + "; filelink_session=" + token).get_status_code(), 401);
    ASSERT_TRUE(auth.logout(token));
    EXPECT_EQ(current_user(server, "filelink_session=" + token).get_status_code(), 401);
}

TEST_F(AuthApiTest, LogsOutAndClearsSessionCookie) {
    HttpServer server("127.0.0.1", 9999);
    AuthService auth(pool_);
    AuthApiRouter router(server, auth);
    router.register_routes();
    const HttpResponse registration = register_user(server,
        R"({"username":"alice","password":"correct-password"})");
    ASSERT_EQ(registration.get_status_code(), 201);

    const std::string token = session_token(registration);
    const HttpResponse response = logout(server, "filelink_session=" + token);

    EXPECT_EQ(response.get_status_code(), 204);
    EXPECT_EQ(response.get_headers().at("Set-Cookie"),
        "filelink_session=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0");
    EXPECT_EQ(current_user(server, "filelink_session=" + token).get_status_code(), 401);
}

TEST_F(AuthApiTest, LogoutWithoutValidSessionIsIdempotent) {
    HttpServer server("127.0.0.1", 9999);
    AuthService auth(pool_);
    AuthApiRouter router(server, auth);
    router.register_routes();

    const HttpResponse response = logout(server, "");
    const HttpResponse malformed_response = logout(server, "filelink_session=not-a-token");

    EXPECT_EQ(response.get_status_code(), 204);
    EXPECT_EQ(malformed_response.get_status_code(), 204);
    EXPECT_EQ(response.get_headers().at("Set-Cookie"),
        "filelink_session=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0");
}

} // namespace filelink
