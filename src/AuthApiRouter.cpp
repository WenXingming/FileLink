#include "AuthApiRouter.h"

#include "AuthService.h"

#include <nlohmann/json.hpp>

namespace filelink {

namespace {

HttpResponse json_response(int status_code, const char* status_message,
    const nlohmann::json& body) {
    HttpResponse response;
    response.set_status(status_code, status_message);
    response.set_header("Content-Type", "application/json");
    response.set_body(body.dump());
    return response;
}

HttpResponse authenticated_response(const AuthenticatedSession& session,
    int status_code, const char* status_message) {
    HttpResponse response = json_response(status_code, status_message, {{"username", session.username}});
    response.set_header("Set-Cookie", "filelink_session=" + session.session_token
        + "; Path=/; HttpOnly; SameSite=Lax; Max-Age=604800");
    return response;
}

HttpResponse logged_out_response() {
    HttpResponse response;
    response.set_status(204, "No Content");
    response.set_header("Set-Cookie",
        "filelink_session=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0");
    return response;
}

bool read_credentials(const HttpRequest& request, std::string& username, std::string& password) {
    try {
        const nlohmann::json body = nlohmann::json::parse(request.get_body());
        if (!body.is_object() || !body.contains("username") || !body.contains("password")
            || !body.at("username").is_string() || !body.at("password").is_string()) {
            return false;
        }

        username = body.at("username").get<std::string>();
        password = body.at("password").get<std::string>();
        return true;
    } catch (const nlohmann::json::exception&) {
        return false;
    }
}

} // namespace

void AuthApiRouter::register_routes() {
    server_.add_post_route("/auth/register", [this](const HttpRequest& request, HttpResponse& response) {
        handle_register(request, response);
    });
    server_.add_post_route("/auth/login", [this](const HttpRequest& request, HttpResponse& response) {
        handle_login(request, response);
    });
    server_.add_get_route("/auth/me", [this](const HttpRequest& request, HttpResponse& response) {
        handle_current_user(request, response);
    });
    server_.add_post_route("/auth/logout", [this](const HttpRequest& request, HttpResponse& response) {
        handle_logout(request, response);
    });
}

void AuthApiRouter::handle_register(const HttpRequest& request, HttpResponse& response) {
    std::string username;
    std::string password;
    if (!read_credentials(request, username, password)) {
        response = json_response(400, "Bad Request", {{"message", "Invalid request body"}});
        return;
    }

    AuthenticatedSession session;
    switch (auth_service_.register_user(username, password, session)) {
    case RegisterResult::Success:
        response = authenticated_response(session, 201, "Created");
        return;
    case RegisterResult::InvalidUsername:
        response = json_response(400, "Bad Request", {{"message", "Invalid username"}});
        return;
    case RegisterResult::InvalidPassword:
        response = json_response(400, "Bad Request", {{"message", "Invalid password"}});
        return;
    case RegisterResult::UsernameTaken:
        response = json_response(409, "Conflict", {{"message", "Username already exists"}});
        return;
    case RegisterResult::SystemError:
        response = json_response(500, "Internal Server Error", {{"message", "Registration failed"}});
        return;
    }
}

void AuthApiRouter::handle_login(const HttpRequest& request, HttpResponse& response) {
    std::string username;
    std::string password;
    if (!read_credentials(request, username, password)) {
        response = json_response(400, "Bad Request", {{"message", "Invalid request body"}});
        return;
    }

    AuthenticatedSession session;
    switch (auth_service_.login_user(username, password, session)) {
    case LoginResult::Success:
        response = authenticated_response(session, 200, "OK");
        return;
    case LoginResult::InvalidCredentials:
        response = json_response(401, "Unauthorized", {{"message", "Invalid credentials"}});
        return;
    case LoginResult::SystemError:
        response = json_response(500, "Internal Server Error", {{"message", "Login failed"}});
        return;
    }
}

void AuthApiRouter::handle_current_user(const HttpRequest& request, HttpResponse& response) {
    AuthenticatedUser user;
    switch (request_authenticator_.authenticate(request, user)) {
    case RequestAuthResult::Authenticated:
        response = json_response(200, "OK", {{"username", user.username}});
        return;
    case RequestAuthResult::Unauthorized:
        response = json_response(401, "Unauthorized", {{"message", "Unauthorized"}});
        return;
    case RequestAuthResult::SystemError:
        response = json_response(500, "Internal Server Error", {{"message", "Current user lookup failed"}});
        return;
    }
}

void AuthApiRouter::handle_logout(const HttpRequest& request, HttpResponse& response) {
    std::string session_token;
    if (request_authenticator_.session_token(request, session_token)
        && auth_service_.logout(session_token) == LogoutResult::SystemError) {
        response = json_response(500, "Internal Server Error", {{"message", "Logout failed"}});
        return;
    }

    response = logged_out_response();
}

} // namespace filelink
