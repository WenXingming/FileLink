// ============================================================================
// 文件与上传协议响应 View 实现：维护下载 Header 和 Tus 响应格式。
// 不处理站点页面、健康检查或请求解析。
// ============================================================================

#include "ApiResponseView.h"
#include "database/UploadSession.h"

#include <iomanip>
#include <sstream>

namespace filelink {

namespace {

std::string hex_encode(const std::string& bytes) {
    std::stringstream stream;
    stream << std::hex << std::setfill('0');
    for (unsigned char value : bytes) {
        stream << std::setw(2) << static_cast<int>(value);
    }
    return stream.str();
}

std::string download_name(const std::string& display_name) {
    std::string name;
    for (unsigned char value : display_name) {
        name.push_back(value >= 32 && value < 127 && value != '"' && value != '\\' ? value : '_');
    }
    return name.empty() ? "download" : name;
}

std::string escape_json(const std::string& input) {
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

HttpResponse json_response(const std::string& body) {
    HttpResponse response;
    response.set_status(200, "OK");
    response.set_header("Content-Type", "application/json");
    response.set_body(body);
    return response;
}

} // namespace

HttpResponse ApiResponseView::download_redirect(const std::string& objectKey,
    const std::string& displayName) {
    HttpResponse response;
    response.set_status(200, "OK");
    response.set_header("Content-Type", "application/octet-stream");
    response.set_header("Content-Disposition",
        "attachment; filename=\"" + download_name(displayName) + "\"");
    response.set_header("X-Accel-Redirect", "/_filelink_objects/" + objectKey);
    return response;
}

HttpResponse ApiResponseView::tus_options() {
    HttpResponse response;
    response.set_status(204, "No Content");
    response.set_header("Tus-Resumable", "1.0.0");
    response.set_header("Tus-Version", "1.0.0");
    response.set_header("Tus-Max-Size", "10737418240"); // 10GB
    response.set_header("Tus-Extension", "creation,expiration,termination");
    return response;
}

HttpResponse ApiResponseView::tus_head(uint64_t offset, uint64_t length) {
    HttpResponse response;
    response.set_status(200, "OK");
    response.set_header("Tus-Resumable", "1.0.0");
    response.set_header("Upload-Offset", std::to_string(offset));
    response.set_header("Upload-Length", std::to_string(length));
    return response;
}

HttpResponse ApiResponseView::tus_created(const std::string& uploadIdHex, const std::string& host) {
    HttpResponse response;
    response.set_status(201, "Created");
    response.set_header("Tus-Resumable", "1.0.0");
    response.set_header("Location", "http://" + host + "/uploads/" + uploadIdHex);
    return response;
}

HttpResponse ApiResponseView::tus_patched(uint64_t offset) {
    HttpResponse response;
    response.set_status(204, "No Content");
    response.set_header("Tus-Resumable", "1.0.0");
    response.set_header("Upload-Offset", std::to_string(offset));
    return response;
}

HttpResponse ApiResponseView::tus_error(int statusCode, const std::string& statusMessage, const std::string& message) {
    HttpResponse response;
    response.set_status(statusCode, statusMessage);
    response.set_header("Tus-Resumable", "1.0.0");
    response.set_body(message);
    return response;
}

HttpResponse ApiResponseView::tus_session_status(const db::UploadSession& session) {
    const std::string upload_id_hex = hex_encode(session.upload_id);
    const std::string content_hash_hex = session.has_content_hash
        ? hex_encode(session.content_hash) : "";
    const std::string completed_file_id_hex = session.has_completed_file_id
        ? hex_encode(session.completed_file_id) : "";

    std::string body = "{";
    body += R"("upload_id":")" + upload_id_hex + R"(",)";
    body += R"("state":")" + session.state + R"(",)";
    body += R"("file_name":")" + escape_json(session.file_name) + R"(",)";
    body += R"("total_size":)" + std::to_string(session.total_size) + ",";
    body += R"("committed_offset":)" + std::to_string(session.committed_offset);
    if (session.has_content_hash) {
        body += R"(,"content_hash":")" + content_hash_hex + R"(")";
    }
    if (session.has_completed_file_id) {
        body += R"(,"file_id":")" + completed_file_id_hex + R"(")";
    }
    if (session.has_failure_reason) {
        body += R"(,"failure_reason":")" + escape_json(session.failure_reason) + R"(")";
    }
    body += "}";

    return json_response(body);
}

} // namespace filelink
