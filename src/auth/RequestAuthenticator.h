#pragma once

#include "tudou/http/HttpRequest.h"

#include <string>

namespace filelink {

class AuthService;
struct AuthenticatedUser;

// =====================================================================
// RequestAuthResult：从 HTTP 请求解析当前用户身份后的处理结果。
// =====================================================================
enum class RequestAuthResult {
    Authenticated,
    Unauthorized,
    SystemError
};

// =================================================================
// RequestAuthenticator：HTTP 请求拦截鉴权。从 Cookie 提取会话令牌，并解析当前认证用户。
// =================================================================
class RequestAuthenticator {
public:
    explicit RequestAuthenticator(AuthService& auth_service) : auth_service_(auth_service) {}

    RequestAuthResult authenticate(const HttpRequest& request, AuthenticatedUser& out_user) const;
    bool session_token(const HttpRequest& request, std::string& out_token) const;

private:
    AuthService& auth_service_;
};

} // namespace filelink
