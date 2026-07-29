// ============================================================================
// 文件响应 View 实现：集中维护文件列表、通用错误和删除成功响应。
// Router 只选择响应种类，不拼装 JSON 或状态行。
// ============================================================================

#include "FileResponseView.h"

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

HttpResponse json_response(int status_code, const char* status_message,
    const nlohmann::json& body) {
    HttpResponse response;
    response.set_status(status_code, status_message);
    response.set_header("Content-Type", "application/json");
    response.set_body(body.dump());
    return response;
}

} // namespace

HttpResponse FileResponseView::file_list(const std::vector<db::File>& files) {
    nlohmann::json body = {{"files", nlohmann::json::array()}};
    for (const db::File& file : files) {
        body["files"].push_back({
            {"file_id", hex_encode(file.file_id)},
            {"name", file.display_name}
        });
    }
    return json_response(200, "OK", body);
}

HttpResponse FileResponseView::deleted() {
    HttpResponse response;
    response.set_status(204, "No Content");
    return response;
}

HttpResponse FileResponseView::unauthorized() {
    return json_response(401, "Unauthorized", {{"message", "Unauthorized"}});
}

HttpResponse FileResponseView::not_found() {
    return json_response(404, "Not Found", {{"message", "Not Found"}});
}

HttpResponse FileResponseView::server_error(const std::string& message) {
    return json_response(500, "Internal Server Error", {{"message", message}});
}

} // namespace filelink
