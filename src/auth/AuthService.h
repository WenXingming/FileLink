// ============================================================================
// Model/Application Service（MVC）：认证业务。认证业务接口：定义注册、登录、当前用户和注销用例及其结果类型。
// AuthService 编排数据库与可选会话缓存，不依赖 HTTP 请求或响应。
// ============================================================================

#pragma once

#include <string>

namespace soci {
class connection_pool;
}

namespace filelink {

namespace redis {
class UserSessionCache;
}

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

// 注册或登录成功时返回给客户端的用户与会话令牌。
struct AuthenticatedSession {
    std::string user_id;
    std::string username;
    std::string session_token;
};

// 从已有会话解析出的当前用户身份。
struct AuthenticatedUser {
    std::string user_id;
    std::string username;
};

class AuthService {
public:
    AuthService(soci::connection_pool& pool, redis::UserSessionCache* session_cache = nullptr);

    RegisterResult register_user(const std::string& username, const std::string& password, AuthenticatedSession& out_session);
    LoginResult login_user(const std::string& username, const std::string& password, AuthenticatedSession& out_session);
    CurrentUserResult current_user(const std::string& session_token, AuthenticatedUser& out_user);
    bool logout(const std::string& session_token);

private:
    soci::connection_pool& pool_;
    redis::UserSessionCache* session_cache_; // 可选的非持有缓存。
};

} // namespace filelink
