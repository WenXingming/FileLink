// ============================================================================
// Site 响应 View 实现：集中维护站点端点对外可见的 HTTP 表示。
// Router 只选择响应种类，不拼装 Header、JSON 或 MIME 类型。
// ============================================================================

#include "SiteResponseView.h"

#include "tudou/http/HttpResponse.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>

namespace filelink {

namespace {

std::string file_extension(const std::string& path) {
    const std::size_t dot_position = path.find_last_of('.');
    if (dot_position == std::string::npos) {
        return {};
    }

    std::string extension = path.substr(dot_position);
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return extension;
}

const char* mime_type(const std::string& extension) {
    if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
    if (extension == ".png") return "image/png";
    if (extension == ".gif") return "image/gif";
    if (extension == ".txt") return "text/plain; charset=utf-8";
    if (extension == ".html") return "text/html; charset=utf-8";
    if (extension == ".css") return "text/css; charset=utf-8";
    if (extension == ".js") return "application/javascript; charset=utf-8";
    if (extension == ".json") return "application/json";
    if (extension == ".pdf") return "application/pdf";
    if (extension == ".mp4") return "video/mp4";
    if (extension == ".svg") return "image/svg+xml";
    return "application/octet-stream";
}

HttpResponse error_response(int status_code, const char* status_message, const std::string& message) {
    HttpResponse response;
    response.set_status(status_code, status_message);
    response.set_header("Content-Type", "application/json");
    response.set_body("{\"status\":\"error\",\"message\":" + nlohmann::json(message).dump() + "}");
    return response;
}

} // namespace

HttpResponse SiteResponseView::health_check() {
    HttpResponse response;
    response.set_status(200, "OK");
    response.set_header("Content-Type", "application/json");
    response.set_body(R"({"status":"ok"})");
    return response;
}

HttpResponse SiteResponseView::asset(const std::string& path, const std::string& content) {
    HttpResponse response;
    response.set_status(200, "OK");
    response.set_header("Content-Type", mime_type(file_extension(path)));
    response.set_header("Content-Length", std::to_string(content.size()));
    response.set_body(content);
    return response;
}

HttpResponse SiteResponseView::forbidden(const std::string& message) {
    return error_response(403, "Forbidden", message);
}

HttpResponse SiteResponseView::not_found(const std::string& message) {
    return error_response(404, "Not Found", message);
}

} // namespace filelink
