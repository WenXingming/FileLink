#include "FileApiRouter.h"

#include "ApiResponseView.h"
#include "FileService.h"
#include "auth/AuthRequestParser.h"
#include "auth/AuthService.h"
#include "shares/ShareApiRouter.h"
#include "storage/ObjectStore.h"

#include <nlohmann/json.hpp>

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

bool hex_decode(const std::string& encoded, std::string& out_bytes) {
    if (encoded.size() != 32) {
        return false;
    }

    out_bytes.clear();
    out_bytes.reserve(16);
    for (std::size_t index = 0; index < encoded.size(); index += 2) {
        const char high = encoded[index];
        const char low = encoded[index + 1];
        const auto digit = [](char value) -> int {
            if (value >= '0' && value <= '9') return value - '0';
            if (value >= 'a' && value <= 'f') return value - 'a' + 10;
            return -1;
        };
        const int upper = digit(high);
        const int lower = digit(low);
        if (upper < 0 || lower < 0) {
            return false;
        }
        out_bytes.push_back(static_cast<char>((upper << 4) | lower));
    }
    return true;
}

bool parse_file_id_path(const std::string& path, const std::string& suffix, std::string& out_file_id) {
    const std::string prefix = "/files/";
    if (path.size() != prefix.size() + 32 + suffix.size()
        || path.compare(0, prefix.size(), prefix) != 0
        || path.compare(path.size() - suffix.size(), suffix.size(), suffix) != 0) {
        return false;
    }
    return hex_decode(path.substr(prefix.size(), 32), out_file_id);
}

HttpResponse json_response(int status_code, const char* status_message,
    const nlohmann::json& body) {
    HttpResponse response;
    response.set_status(status_code, status_message);
    response.set_header("Content-Type", "application/json");
    response.set_body(body.dump());
    return response;
}

bool authenticate_request(AuthService& auth_service, const HttpRequest& request,
    AuthenticatedUser& out_user, HttpResponse& response) {
    std::string session_token;
    if (!AuthRequestParser::parse_session_token(request, session_token)) {
        response = json_response(401, "Unauthorized", {{"message", "Unauthorized"}});
        return false;
    }

    switch (auth_service.current_user(session_token, out_user)) {
    case CurrentUserResult::Success:
        return true;
    case CurrentUserResult::InvalidSession:
        response = json_response(401, "Unauthorized", {{"message", "Unauthorized"}});
        return false;
    case CurrentUserResult::SystemError:
        response = json_response(500, "Internal Server Error", {{"message", "Authentication failed"}});
        return false;
    }
    return false;
}

} // namespace

void FileApiRouter::register_routes() {
    server_.add_get_route("/files", [this](const HttpRequest& request, HttpResponse& response) {
        handle_list_files(request, response);
    });
    server_.add_prefix_route("/files/", [this](const HttpRequest& request, HttpResponse& response) {
        if (share_api_router_.handle_management_request(request, response)) {
            return;
        }
        if (request.get_method() == "GET") {
            handle_download(request, response);
        } else if (request.get_method() == "DELETE") {
            handle_delete(request, response);
        } else {
            response = json_response(404, "Not Found", {{"message", "Not Found"}});
        }
    });
}

void FileApiRouter::handle_download(const HttpRequest& request, HttpResponse& response) {
    const std::string suffix = "/download";
    const std::string path = request.get_path();
    std::string file_id;
    if (request.get_method() != "GET" || !parse_file_id_path(path, suffix, file_id)) {
        response = json_response(404, "Not Found", {{"message", "Not Found"}});
        return;
    }

    AuthenticatedUser user;
    if (!authenticate_request(auth_service_, request, user, response)) return;

    try {
        db::File file;
        if (!file_service_.find_file(user.user_id, file_id, file)) {
            response = json_response(404, "Not Found", {{"message", "Not Found"}});
            return;
        }

        const std::string object_key = object_store_.get_object_key(hex_encode(file.content_hash));
        response = ApiResponseView::download_redirect(object_key, file.display_name);
    } catch (const std::exception&) {
        response = json_response(500, "Internal Server Error", {{"message", "File download failed"}});
    }
}

void FileApiRouter::handle_delete(const HttpRequest& request, HttpResponse& response) {
    std::string file_id;
    if (!parse_file_id_path(request.get_path(), "", file_id)) {
        response = json_response(404, "Not Found", {{"message", "Not Found"}});
        return;
    }

    AuthenticatedUser user;
    if (!authenticate_request(auth_service_, request, user, response)) return;

    try {
        if (!file_service_.delete_file(user.user_id, file_id)) {
            response = json_response(404, "Not Found", {{"message", "Not Found"}});
            return;
        }
        response.set_status(204, "No Content");
    } catch (const std::exception&) {
        response = json_response(500, "Internal Server Error", {{"message", "File deletion failed"}});
    }
}

void FileApiRouter::handle_list_files(const HttpRequest& request, HttpResponse& response) {
    AuthenticatedUser user;
    if (!authenticate_request(auth_service_, request, user, response)) return;

    try {
        std::vector<db::File> files;
        file_service_.list_files(user.user_id, files);

        nlohmann::json body = {{"files", nlohmann::json::array()}};
        for (const db::File& file : files) {
            body["files"].push_back({
                {"file_id", hex_encode(file.file_id)},
                {"name", file.display_name}
            });
        }
        response = json_response(200, "OK", body);
    } catch (const std::exception&) {
        response = json_response(500, "Internal Server Error", {{"message", "File listing failed"}});
    }
}

} // namespace filelink
