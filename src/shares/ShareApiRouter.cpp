#include "ShareApiRouter.h"

#include "ApiResponseView.h"
#include "ShareService.h"
#include "auth/AuthService.h"
#include "storage/ObjectStore.h"

#include <nlohmann/json.hpp>

#include <ctime>
#include <iomanip>
#include <limits>
#include <sstream>

namespace filelink {

namespace {

const std::string kFilesPrefix = "/files/";
const std::string kSharesSuffix = "/shares";
const std::string kPublicSharesPrefix = "/shares/";
const std::string kDownloadSuffix = "/download";
const std::size_t kIdHexLength = 32;
const std::time_t kMaximumShareLifetime = 30 * 24 * 60 * 60;

HttpResponse json_response(int status_code, const char* status_message,
    const nlohmann::json& body) {
    HttpResponse response;
    response.set_status(status_code, status_message);
    response.set_header("Content-Type", "application/json");
    response.set_body(body.dump());
    return response;
}

std::string hex_encode(const std::string& bytes) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (unsigned char value : bytes) {
        stream << std::setw(2) << static_cast<int>(value);
    }
    return stream.str();
}

bool hex_decode(const std::string& encoded, std::string& out_bytes) {
    if (encoded.size() != kIdHexLength) {
        return false;
    }

    out_bytes.clear();
    out_bytes.reserve(kIdHexLength / 2);
    for (std::size_t index = 0; index < encoded.size(); index += 2) {
        const auto digit = [](char value) -> int {
            if (value >= '0' && value <= '9') return value - '0';
            if (value >= 'a' && value <= 'f') return value - 'a' + 10;
            return -1;
        };
        const int high = digit(encoded[index]);
        const int low = digit(encoded[index + 1]);
        if (high < 0 || low < 0) {
            return false;
        }
        out_bytes.push_back(static_cast<char>((high << 4) | low));
    }
    return true;
}

bool parse_share_path(const std::string& path, std::string& out_file_id,
    std::string& out_share_id, bool& out_is_collection) {
    if (path.size() < kFilesPrefix.size() + kIdHexLength + kSharesSuffix.size()
        || path.compare(0, kFilesPrefix.size(), kFilesPrefix) != 0) {
        return false;
    }

    const std::string file_id = path.substr(kFilesPrefix.size(), kIdHexLength);
    const std::size_t suffix_offset = kFilesPrefix.size() + kIdHexLength;
    if (path.compare(suffix_offset, kSharesSuffix.size(), kSharesSuffix) != 0
        || !hex_decode(file_id, out_file_id)) {
        return false;
    }

    if (path.size() == suffix_offset + kSharesSuffix.size()) {
        out_share_id.clear();
        out_is_collection = true;
        return true;
    }

    if (path.size() != suffix_offset + kSharesSuffix.size() + 1 + kIdHexLength
        || path[suffix_offset + kSharesSuffix.size()] != '/') {
        return false;
    }

    out_is_collection = false;
    return hex_decode(path.substr(path.size() - kIdHexLength), out_share_id);
}

bool authenticate_request(RequestAuthenticator& authenticator, const HttpRequest& request,
    AuthenticatedUser& out_user, HttpResponse& response) {
    switch (authenticator.authenticate(request, out_user)) {
    case RequestAuthResult::Authenticated:
        return true;
    case RequestAuthResult::Unauthorized:
        response = json_response(401, "Unauthorized", {{"message", "Unauthorized"}});
        return false;
    case RequestAuthResult::SystemError:
        response = json_response(500, "Internal Server Error", {{"message", "Authentication failed"}});
        return false;
    }
    return false;
}

bool read_expiry(const HttpRequest& request, std::time_t& out_expiry) {
    try {
        const nlohmann::json body = nlohmann::json::parse(request.get_body());
        if (!body.is_object() || !body.contains("expires_in_seconds")
            || !body.at("expires_in_seconds").is_number_integer()) {
            return false;
        }

        const int64_t lifetime = body.at("expires_in_seconds").get<int64_t>();
        if (lifetime <= 0 || lifetime > kMaximumShareLifetime) {
            return false;
        }

        const std::time_t now = std::time(nullptr);
        if (now > std::numeric_limits<std::time_t>::max() - lifetime) {
            return false;
        }
        out_expiry = now + lifetime;
        return true;
    } catch (const nlohmann::json::exception&) {
        return false;
    }
}

std::time_t unix_time(std::tm value) {
    return std::mktime(&value);
}

bool public_token_from_path(const std::string& path, std::string& out_token) {
    if (path.size() <= kPublicSharesPrefix.size() + kDownloadSuffix.size()
        || path.compare(0, kPublicSharesPrefix.size(), kPublicSharesPrefix) != 0
        || path.compare(path.size() - kDownloadSuffix.size(), kDownloadSuffix.size(), kDownloadSuffix) != 0) {
        return false;
    }

    out_token = path.substr(kPublicSharesPrefix.size(),
        path.size() - kPublicSharesPrefix.size() - kDownloadSuffix.size());
    return !out_token.empty() && out_token.find('/') == std::string::npos;
}

} // namespace

void ShareApiRouter::register_public_routes() {
    server_.add_prefix_route(kPublicSharesPrefix, [this](const HttpRequest& request, HttpResponse& response) {
        handle_public_download(request, response);
    });
}

bool ShareApiRouter::handle_management_request(const HttpRequest& request, HttpResponse& response) {
    std::string file_id;
    std::string share_id;
    bool is_collection = false;
    if (!parse_share_path(request.get_path(), file_id, share_id, is_collection)) {
        return false;
    }

    if (is_collection && request.get_method() == "POST") {
        handle_create(request, response, file_id);
    } else if (is_collection && request.get_method() == "GET") {
        handle_list(request, response, file_id);
    } else if (!is_collection && request.get_method() == "DELETE") {
        handle_revoke(request, response, file_id, share_id);
    } else {
        response = json_response(404, "Not Found", {{"message", "Not Found"}});
    }
    return true;
}

void ShareApiRouter::handle_public_download(const HttpRequest& request, HttpResponse& response) {
    std::string token;
    if (request.get_method() != "GET" || !public_token_from_path(request.get_path(), token)) {
        response = json_response(404, "Not Found", {{"message", "Not Found"}});
        return;
    }

    try {
        db::File file;
        if (!share_service_.find_shared_file(token, file)) {
            response = json_response(404, "Not Found", {{"message", "Not Found"}});
            return;
        }

        const std::string object_key = object_store_.get_object_key(hex_encode(file.content_hash));
        response = ApiResponseView::download_redirect(object_key, file.display_name);
    } catch (const std::exception&) {
        response = json_response(500, "Internal Server Error", {{"message", "Share download failed"}});
    }
}

void ShareApiRouter::handle_create(const HttpRequest& request, HttpResponse& response,
    const std::string& file_id) {
    AuthenticatedUser user;
    if (!authenticate_request(request_authenticator_, request, user, response)) return;

    std::time_t expiry;
    if (!read_expiry(request, expiry)) {
        response = json_response(400, "Bad Request", {{"message", "Invalid expires_in_seconds"}});
        return;
    }

    CreatedShare share;
    switch (share_service_.create_share(user.user_id, file_id, expiry, share)) {
    case CreateShareResult::Success:
        response = json_response(201, "Created", {
            {"share_id", hex_encode(share.share_id)},
            {"token", share.token},
            {"expires_at", unix_time(share.expires_at)}
        });
        return;
    case CreateShareResult::FileNotFound:
        response = json_response(404, "Not Found", {{"message", "Not Found"}});
        return;
    case CreateShareResult::InvalidExpiry:
        response = json_response(400, "Bad Request", {{"message", "Invalid expires_in_seconds"}});
        return;
    case CreateShareResult::SystemError:
        response = json_response(500, "Internal Server Error", {{"message", "Share creation failed"}});
        return;
    }
}

void ShareApiRouter::handle_list(const HttpRequest& request, HttpResponse& response,
    const std::string& file_id) {
    AuthenticatedUser user;
    if (!authenticate_request(request_authenticator_, request, user, response)) return;

    try {
        std::vector<db::Share> shares;
        if (!share_service_.list_shares(user.user_id, file_id, shares)) {
            response = json_response(404, "Not Found", {{"message", "Not Found"}});
            return;
        }

        nlohmann::json body = {{"shares", nlohmann::json::array()}};
        for (const db::Share& share : shares) {
            body["shares"].push_back({
                {"share_id", hex_encode(share.share_id)},
                {"expires_at", unix_time(share.expires_at)}
            });
        }
        response = json_response(200, "OK", body);
    } catch (const std::exception&) {
        response = json_response(500, "Internal Server Error", {{"message", "Share listing failed"}});
    }
}

void ShareApiRouter::handle_revoke(const HttpRequest& request, HttpResponse& response,
    const std::string& file_id, const std::string& share_id) {
    AuthenticatedUser user;
    if (!authenticate_request(request_authenticator_, request, user, response)) return;

    switch (share_service_.revoke_share(user.user_id, file_id, share_id)) {
    case RevokeShareResult::Success:
        response.set_status(204, "No Content");
        return;
    case RevokeShareResult::FileNotFound:
    case RevokeShareResult::ShareNotFound:
        response = json_response(404, "Not Found", {{"message", "Not Found"}});
        return;
    case RevokeShareResult::SystemError:
        response = json_response(500, "Internal Server Error", {{"message", "Share revocation failed"}});
        return;
    }
}

} // namespace filelink
