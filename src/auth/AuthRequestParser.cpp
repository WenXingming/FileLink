// ============================================================================
// 认证请求解析实现：解析登录注册 JSON，并从 Cookie Header 提取会话令牌。
// 所有解析失败统一返回 false，具体 HTTP 语义由调用它的 Router 决定。
// ============================================================================

#include "AuthRequestParser.h"

#include "tudou/http/HttpRequest.h"

#include <nlohmann/json.hpp>

#include <cctype>
#include <sstream>

namespace filelink {

namespace {

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

} // namespace

bool AuthRequestParser::parse_credentials(const HttpRequest& request, std::string& username, std::string& password) {
    try {
        const nlohmann::json body = nlohmann::json::parse(request.get_body());
        username = body.at("username").get<std::string>();
        password = body.at("password").get<std::string>();
        return true;
    } catch (const nlohmann::json::exception&) {
        return false;
    }
}

bool AuthRequestParser::parse_session_token(const HttpRequest& request, std::string& session_token) {
    std::istringstream cookies(request.get_header("Cookie"));
    std::string cookie;
    bool found_session_cookie = false;

    while (std::getline(cookies, cookie, ';')) {
        const std::size_t equals = cookie.find('=');
        if (equals == std::string::npos || trim_whitespace(cookie.substr(0, equals)) != "filelink_session") {
            continue;
        }

        // 多个同名会话 Cookie 含义不明确，不任意选择其中一个。
        if (found_session_cookie) {
            return false;
        }
        session_token = trim_whitespace(cookie.substr(equals + 1));
        found_session_cookie = true;
    }

    return found_session_cookie && !session_token.empty();
}

} // namespace filelink
