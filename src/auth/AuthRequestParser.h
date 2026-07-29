// ============================================================================
// 认证请求解析器：把 HttpRequest 中的 JSON 与 Cookie 转换为认证业务输入。
// 只判断输入能否解析，不验证用户或会话，也不决定 HTTP 响应。
// ============================================================================

#pragma once

#include <string>

class HttpRequest;

namespace filelink {

class AuthRequestParser {
public:
    static bool parse_credentials(const HttpRequest& request, std::string& username, std::string& password);
    static bool parse_session_token(const HttpRequest& request, std::string& session_token);
};

} // namespace filelink
