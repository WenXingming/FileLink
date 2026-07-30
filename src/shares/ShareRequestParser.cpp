// ============================================================================
// 分享请求解析器实现：校验固定 URL 形状，并把十六进制 ID 解码为二进制值。
// `expires_in_seconds` 当前允许 1 秒至 30 天。
// ============================================================================

#include "ShareRequestParser.h"

#include "tudou/http/HttpRequest.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <limits>

namespace filelink {

namespace {

const std::string kSharesPrefix = "/shares/";
const std::size_t kIdHexLength = 32;
const std::time_t kMaximumShareLifetime = 30 * 24 * 60 * 60;

int hex_digit(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    return -1;
}

bool decode_id(const std::string& encoded, std::string& id) {
    if (encoded.size() != kIdHexLength) {
        return false;
    }

    id.clear();
    id.reserve(kIdHexLength / 2);
    for (std::size_t index = 0; index < encoded.size(); index += 2) {
        const int high = hex_digit(encoded[index]);
        const int low = hex_digit(encoded[index + 1]);
        if (high < 0 || low < 0) {
            return false;
        }
        id.push_back(static_cast<char>((high << 4) | low));
    }
    return true;
}

std::size_t collection_path_size() {
    return kSharesPrefix.size() + kIdHexLength;
}

bool parse_collection_path(const std::string& path, std::string& file_id) {
    if (path.size() != collection_path_size()
        || path.compare(0, kSharesPrefix.size(), kSharesPrefix) != 0) {
        return false;
    }
    return decode_id(path.substr(kSharesPrefix.size(), kIdHexLength), file_id);
}

} // namespace

bool ShareRequestParser::parse_collection_file_id(const HttpRequest& request,
    std::string& file_id) {
    return parse_collection_path(request.get_path(), file_id);
}

bool ShareRequestParser::parse_share_ids(const HttpRequest& request, std::string& file_id,
    std::string& share_id) {
    const std::string path = request.get_path();
    if (path.size() != collection_path_size() + 1 + kIdHexLength
        || path[collection_path_size()] != '/') {
        return false;
    }

    return parse_collection_path(path.substr(0, collection_path_size()), file_id)
        && decode_id(path.substr(collection_path_size() + 1), share_id);
}

bool ShareRequestParser::parse_expiry(const HttpRequest& request, std::time_t& expiry) {
    try {
        const nlohmann::json body = nlohmann::json::parse(request.get_body());
        if (!body.is_object() || !body.contains("expires_in_seconds")
            || !body.at("expires_in_seconds").is_number_integer()) {
            return false;
        }

        const int64_t lifetime = body.at("expires_in_seconds").get<int64_t>();
        const std::time_t now = std::time(nullptr);
        if (lifetime <= 0 || lifetime > kMaximumShareLifetime
            || now > std::numeric_limits<std::time_t>::max() - lifetime) {
            return false;
        }
        expiry = now + lifetime;
        return true;
    } catch (const nlohmann::json::exception&) {
        return false;
    }
}

} // namespace filelink
