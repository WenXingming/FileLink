#include "AuthApiRouter.h"

#include "AuthService.h"

#include <nlohmann/json.hpp>

#include <cctype>

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

std::string trim_whitespace(const std::string& value) {
    std::size_t first = 0;
    while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first]))) {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1]))) {
        --last;
    }
    return value.substr(first, last - first);
}

bool read_session_token(const HttpRequest& request, std::string& session_token) {
    const std::string& cookies = request.get_header("Cookie");
    bool found = false;
    std::size_t begin = 0;

    while (begin <= cookies.size()) {
        const std::size_t end = cookies.find(';', begin);
        const std::string cookie = trim_whitespace(cookies.substr(begin, end - begin));
        const std::size_t equals = cookie.find('=');
        if (equals != std::string::npos && trim_whitespace(cookie.substr(0, equals)) == "filelink_session") {
            if (found) {
                return false;
            }
            session_token = trim_whitespace(cookie.substr(equals + 1));
            found = true;
        }

        if (end == std::string::npos) {
            break;
        }
        begin = end + 1;
    }

    return found && !session_token.empty();
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
    std::string session_token;
    if (!read_session_token(request, session_token)) {
        response = json_response(401, "Unauthorized", {{"message", "Unauthorized"}});
        return;
    }

    AuthenticatedUser user;
    switch (auth_service_.current_user(session_token, user)) {
    case CurrentUserResult::Success:
        response = json_response(200, "OK", {{"username", user.username}});
        return;
    case CurrentUserResult::InvalidSession:
        response = json_response(401, "Unauthorized", {{"message", "Unauthorized"}});
        return;
    case CurrentUserResult::SystemError:
        response = json_response(500, "Internal Server Error", {{"message", "Current user lookup failed"}});
        return;
    }
}

void AuthApiRouter::handle_logout(const HttpRequest& request, HttpResponse& response) {
    std::string session_token;
    if (read_session_token(request, session_token)
        && auth_service_.logout(session_token) == LogoutResult::SystemError) {
        response = json_response(500, "Internal Server Error", {{"message", "Logout failed"}});
        return;
    }

    response = logged_out_response();
}

} // namespace filelink
