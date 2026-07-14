#pragma once

#include <string>

namespace soci {
class connection_pool;
}

namespace filelink {

namespace redis {
class UserSessionCache;
}

// ========================================================
// RegisterResult：账户注册操作的处理结果。
// ========================================================
enum class RegisterResult {
    Success,
    InvalidUsername,
    InvalidPassword,
    UsernameTaken,
    SystemError
};

// ========================================================
// LoginResult：账户登录操作的处理结果。
// ========================================================
enum class LoginResult {
    Success,
    InvalidCredentials,
    SystemError
};

// =================================================================
// CurrentUserResult：根据会话令牌查询当前用户的处理结果。
// =================================================================
enum class CurrentUserResult {
    Success,
    InvalidSession,
    SystemError
};

// ========================================================
// LogoutResult：退出登录操作的处理结果。
// ========================================================
enum class LogoutResult {
    Success,
    InvalidSession,
    SystemError
};

// =============================================================================
// AuthenticatedSession：注册或登录成功后返回的用户信息及原始会话令牌。
// =============================================================================
struct AuthenticatedSession {
    std::string user_id;
    std::string username;
    std::string session_token;
};

// =====================================================================
// AuthenticatedUser：通过有效会话令牌解析得到的当前用户身份信息。
// =====================================================================
struct AuthenticatedUser {
    std::string user_id;
    std::string username;
};

// =================================================================
// AuthService：处理账户注册、登录、服务端会话查询和退出登录。
// =================================================================
class AuthService {
public:
    AuthService(soci::connection_pool& pool, redis::UserSessionCache* session_cache = nullptr)
        : pool_(pool), session_cache_(session_cache) {}

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
    redis::UserSessionCache* session_cache_;
};

} // namespace filelink
