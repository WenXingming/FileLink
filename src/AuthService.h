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

struct Registration {
    std::string user_id;
    std::string username;
    std::string session_token;
};

class AuthService {
public:
    explicit AuthService(soci::connection_pool& pool) : pool_(pool) {}

    RegisterResult register_user(const std::string& username,
        const std::string& password,
        Registration& out_registration);

private:
    soci::connection_pool& pool_;
};

} // namespace filelink
