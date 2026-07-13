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

struct AuthenticatedSession {
    std::string user_id;
    std::string username;
    std::string session_token;
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

private:
    soci::connection_pool& pool_;
};

} // namespace filelink
