#pragma once

#include <string>

namespace soci {
class connection_pool;
}

namespace filelink {

enum class RegisterResult {
    Success,
    InvalidUsername,
    InvalidPassword,
    UsernameTaken,
    SystemError
};

enum class LoginResult {
    Success,
    InvalidCredentials,
    SystemError
};

enum class CurrentUserResult {
    Success,
    InvalidSession,
    SystemError
};

enum class LogoutResult {
    Success,
    InvalidSession,
    SystemError
};

struct AuthenticatedSession {
    std::string user_id;
    std::string username;
    std::string session_token;
};

struct AuthenticatedUser {
    std::string user_id;
    std::string username;
};

class AuthService {
public:
    explicit AuthService(soci::connection_pool& pool) : pool_(pool) {}

    RegisterResult register_user(const std::string& username,
        const std::string& password,
        AuthenticatedSession& out_session);
    LoginResult login_user(const std::string& username,
        const std::string& password,
        AuthenticatedSession& out_session);
    CurrentUserResult current_user(const std::string& session_token,
        AuthenticatedUser& out_user);
    LogoutResult logout(const std::string& session_token);

private:
    soci::connection_pool& pool_;
};

} // namespace filelink
