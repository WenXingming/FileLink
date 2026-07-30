// ============================================================================
// 分享响应 View 实现：集中维护分享 JSON、状态码和错误消息。
// Router 只选择响应种类，不拼装 JSON 或状态行。
// ============================================================================

#include "ShareResponseView.h"

#include "ShareService.h"
#include "database/Share.h"
#include "tudou/http/HttpResponse.h"

#include <nlohmann/json.hpp>

#include <iomanip>
#include <sstream>

namespace filelink {

namespace {

std::string hex_encode(const std::string& bytes) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (unsigned char value : bytes) {
        stream << std::setw(2) << static_cast<int>(value);
    }
    return stream.str();
}

std::time_t unix_time(std::tm value) {
    return std::mktime(&value);
}

HttpResponse json_response(int status_code, const char* status_message,
    const nlohmann::json& body) {
    HttpResponse response;
    response.set_status(status_code, status_message);
    response.set_header("Content-Type", "application/json");
    response.set_body(body.dump());
    return response;
}

} // namespace

HttpResponse ShareResponseView::created(const CreatedShare& share) {
    return json_response(201, "Created", {
        {"share_id", hex_encode(share.share_id)},
        {"token", share.token},
        {"expires_at", unix_time(share.expires_at)}
    });
}

HttpResponse ShareResponseView::share_list(const std::vector<db::Share>& shares) {
    nlohmann::json body = {{"shares", nlohmann::json::array()}};
    for (const db::Share& share : shares) {
        body["shares"].push_back({
            {"share_id", hex_encode(share.share_id)},
            {"expires_at", unix_time(share.expires_at)}
        });
    }
    return json_response(200, "OK", body);
}

HttpResponse ShareResponseView::revoked() {
    HttpResponse response;
    response.set_status(204, "No Content");
    return response;
}

HttpResponse ShareResponseView::invalid_expiry() {
    return json_response(400, "Bad Request", {{"message", "Invalid expires_in_seconds"}});
}

HttpResponse ShareResponseView::unauthorized() {
    return json_response(401, "Unauthorized", {{"message", "Unauthorized"}});
}

HttpResponse ShareResponseView::not_found() {
    return json_response(404, "Not Found", {{"message", "Not Found"}});
}

HttpResponse ShareResponseView::server_error(const std::string& message) {
    return json_response(500, "Internal Server Error", {{"message", message}});
}

} // namespace filelink
