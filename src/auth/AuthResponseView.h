// ============================================================================
// View（MVC）：认证响应 View：把认证用例结果表示为 JSON、状态码和会话 Cookie。
// 只构造 HttpResponse，不读取请求，也不执行认证业务。
// ============================================================================

#pragma once

class HttpResponse;

namespace filelink {

struct AuthenticatedSession;
struct AuthenticatedUser;

class AuthResponseView {
public:
    static HttpResponse registered(const AuthenticatedSession& session);
    static HttpResponse logged_in(const AuthenticatedSession& session);
    static HttpResponse current_user(const AuthenticatedUser& user);
    static HttpResponse logged_out();
    static HttpResponse error(int status_code, const char* status_message, const char* message);
};

} // namespace filelink
