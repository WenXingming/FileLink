#include "FileApiRouter.h"

#include "FileService.h"
#include "auth/AuthService.h"

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

HttpResponse json_response(int status_code, const char* status_message,
    const nlohmann::json& body) {
    HttpResponse response;
    response.set_status(status_code, status_message);
    response.set_header("Content-Type", "application/json");
    response.set_body(body.dump());
    return response;
}

} // namespace

void FileApiRouter::register_routes() {
    server_.add_get_route("/files", [this](const HttpRequest& request, HttpResponse& response) {
        handle_list_files(request, response);
    });
}

void FileApiRouter::handle_list_files(const HttpRequest& request, HttpResponse& response) {
    AuthenticatedUser user;
    switch (request_authenticator_.authenticate(request, user)) {
    case RequestAuthResult::Authenticated:
        break;
    case RequestAuthResult::Unauthorized:
        response = json_response(401, "Unauthorized", {{"message", "Unauthorized"}});
        return;
    case RequestAuthResult::SystemError:
        response = json_response(500, "Internal Server Error", {{"message", "Authentication failed"}});
        return;
    }

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
