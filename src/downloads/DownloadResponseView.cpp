// ============================================================================
// 下载响应 View 实现：构造 Content-Disposition 和 X-Accel-Redirect。
// 响应体保持为空，实际文件字节由 Nginx 发送。
// ============================================================================

#include "DownloadResponseView.h"

#include "DownloadService.h"
#include "tudou/http/HttpResponse.h"

#include <nlohmann/json.hpp>

namespace filelink {

namespace {

std::string download_name(const std::string& display_name) {
    std::string name;
    for (unsigned char value : display_name) {
        name.push_back(value >= 32 && value < 127 && value != '"' && value != '\\' ? value : '_');
    }
    return name.empty() ? "download" : name;
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

HttpResponse DownloadResponseView::file(const DownloadTarget& target) {
    HttpResponse response;
    response.set_status(200, "OK");
    response.set_header("Content-Type", "application/octet-stream");
    response.set_header("Content-Disposition",
        "attachment; filename=\"" + download_name(target.display_name) + "\"");
    response.set_header("X-Accel-Redirect", "/_filelink_objects/" + target.object_key);
    return response;
}

HttpResponse DownloadResponseView::unauthorized() {
    return json_response(401, "Unauthorized", {{"message", "Unauthorized"}});
}

HttpResponse DownloadResponseView::not_found() {
    return json_response(404, "Not Found", {{"message", "Not Found"}});
}

HttpResponse DownloadResponseView::server_error(const std::string& message) {
    return json_response(500, "Internal Server Error", {{"message", message}});
}

} // namespace filelink
