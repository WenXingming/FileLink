#include "ApiResponseView.h"
#include "database/UploadSession.h"
#include <sstream>
#include <iomanip>

namespace filelink {

namespace {

const char* status_message(int status_code) {
    switch (status_code) {
    case 200:
        return "OK";
    case 400:
        return "Bad Request";
    case 401:
        return "Unauthorized";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 409:
        return "Conflict";
    case 413:
        return "Payload Too Large";
    default:
        return "Internal Server Error";
    }
}

std::string hex_encode(const std::string& bytes) {
    std::stringstream stream;
    stream << std::hex << std::setfill('0');
    for (unsigned char value : bytes) {
        stream << std::setw(2) << static_cast<int>(value);
    }
    return stream.str();
}

} // namespace

HttpResponse ApiResponseView::json(int statusCode, const std::string& jsonBody) {
    HttpResponse response = HttpResponse::plain_text(statusCode, status_message(statusCode), jsonBody);
    response.set_header("Content-Type", "application/json");
    return response;
}

HttpResponse ApiResponseView::health_check() {
    return json(200, R"({"status":"ok"})");
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

    return json(200, body);
}

} // namespace filelink
