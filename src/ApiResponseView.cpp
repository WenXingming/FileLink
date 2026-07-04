#include "ApiResponseView.h"

namespace filelink {

HttpResponse ApiResponseView::json(int statusCode, const std::string& jsonBody) {
    HttpResponse response = HttpResponse::plain_text(statusCode, 
        statusCode == 200 ? "OK" : 
        (statusCode == 400 ? "Bad Request" : "Internal Server Error"), 
        jsonBody);
    response.set_header("Content-Type", "application/json");
    return response;
}

HttpResponse ApiResponseView::health_check() {
    return json(200, R"({"status":"ok"})");
}

HttpResponse ApiResponseView::upload_success(const UploadResult& result, const std::string& host) {
    std::string shareUrl = "http://" + host + "/objects/" + result.hash + result.extension;
    std::string respBody = "{\"status\":\"success\",\"hash\":\"" + result.hash + 
                           "\",\"result\":\"" + result.status + "\",\"url\":\"" + shareUrl + "\"}";
    return json(200, respBody);
}

HttpResponse ApiResponseView::error(int statusCode, const std::string& message) {
    std::string escapedMessage = escape_json(message);
    std::string respBody = "{\"status\":\"error\",\"message\":\"" + escapedMessage + "\"}";
    return json(statusCode, respBody);
}

HttpResponse ApiResponseView::file(const std::string& content, const std::string& extension) {
    HttpResponse response;
    response.set_status(200, "OK");
    response.set_body(content);
    response.set_header("Content-Type", infer_mime_type(extension));
    response.set_header("Content-Length", std::to_string(content.size()));
    return response;
}

std::string ApiResponseView::infer_mime_type(const std::string& ext) {
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".png") return "image/png";
    if (ext == ".gif") return "image/gif";
    if (ext == ".txt") return "text/plain; charset=utf-8";
    if (ext == ".html") return "text/html; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".js") return "application/javascript; charset=utf-8";
    if (ext == ".json") return "application/json";
    if (ext == ".pdf") return "application/pdf";
    if (ext == ".mp4") return "video/mp4";
    if (ext == ".svg") return "image/svg+xml";
    return "application/octet-stream";
}

std::string ApiResponseView::escape_json(const std::string& input) {
    std::string output;
    output.reserve(input.length());
    for (char c : input) {
        if (c == '"') output += "\\\"";
        else if (c == '\\') output += "\\\\";
        else if (c == '\b') output += "\\b";
        else if (c == '\f') output += "\\f";
        else if (c == '\n') output += "\\n";
        else if (c == '\r') output += "\\r";
        else if (c == '\t') output += "\\t";
        else output += c;
    }
    return output;
}

} // namespace filelink
