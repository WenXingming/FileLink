// ============================================================================
// 认证响应 View 实现：集中维护认证 JSON 结构和 Set-Cookie 格式。
// Router 只选择响应种类，不重复拼装认证协议细节。
// ============================================================================

#include "AuthResponseView.h"

#include "AuthService.h"
#include "tudou/http/HttpResponse.h"

#include <nlohmann/json.hpp>

namespace filelink {

namespace {

HttpResponse json_response(int status_code, const char* status_message, const nlohmann::json& body) {
    HttpResponse response;
    response.set_status(status_code, status_message);
    response.set_header("Content-Type", "application/json");
    response.set_body(body.dump());
    return response;
}

HttpResponse authenticated_response(const AuthenticatedSession& session, int status_code, const char* status_message) {
    HttpResponse response = json_response(status_code, status_message, {{"username", session.username}});
    response.set_header("Set-Cookie", "filelink_session=" + session.session_token
        + "; Path=/; HttpOnly; SameSite=Lax; Max-Age=604800");
    return response;
}

} // namespace

HttpResponse AuthResponseView::registered(const AuthenticatedSession& session) {
    return authenticated_response(session, 201, "Created");
}

HttpResponse AuthResponseView::logged_in(const AuthenticatedSession& session) {
    return authenticated_response(session, 200, "OK");
}

HttpResponse AuthResponseView::current_user(const AuthenticatedUser& user) {
    return json_response(200, "OK", {{"username", user.username}});
}

HttpResponse AuthResponseView::logged_out() {
    HttpResponse response;
    response.set_status(204, "No Content");
    response.set_header("Set-Cookie", "filelink_session=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0");
    return response;
}

HttpResponse AuthResponseView::error(int status_code, const char* status_message, const char* message) {
    return json_response(status_code, status_message, {{"message", message}});
}

} // namespace filelink
