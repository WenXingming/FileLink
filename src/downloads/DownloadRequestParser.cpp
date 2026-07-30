// ============================================================================
// 下载请求解析器实现：私有下载使用 16 字节文件 ID，分享下载使用 32 字节 Token。
// 两者在 URL 中均使用固定长度的小写十六进制文本。
// ============================================================================

#include "DownloadRequestParser.h"

#include "tudou/http/HttpRequest.h"

#include <algorithm>

namespace filelink {

namespace {

const std::string kPrivatePrefix = "/downloads/private/";
const std::string kSharedPrefix = "/downloads/shared/";
const std::size_t kFileIdHexLength = 32;
const std::size_t kShareTokenHexLength = 64;

bool is_lowercase_hex(const std::string& value, std::size_t expected_size) {
    return value.size() == expected_size
        && std::all_of(value.begin(), value.end(), [](char digit) {
            return (digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f');
        });
}

int hex_digit(char value) {
    return value <= '9' ? value - '0' : value - 'a' + 10;
}

bool decode_file_id(const std::string& encoded, std::string& file_id) {
    if (!is_lowercase_hex(encoded, kFileIdHexLength)) {
        return false;
    }

    file_id.clear();
    file_id.reserve(kFileIdHexLength / 2);
    for (std::size_t index = 0; index < encoded.size(); index += 2) {
        file_id.push_back(static_cast<char>(
            (hex_digit(encoded[index]) << 4) | hex_digit(encoded[index + 1])));
    }
    return true;
}

bool read_path_value(const std::string& path, const std::string& prefix,
    std::string& value) {
    if (path.size() <= prefix.size() || path.compare(0, prefix.size(), prefix) != 0) {
        return false;
    }
    value = path.substr(prefix.size());
    return value.find('/') == std::string::npos;
}

} // namespace

bool DownloadRequestParser::parse_private_file_id(const HttpRequest& request,
    std::string& file_id) {
    std::string encoded;
    return read_path_value(request.get_path(), kPrivatePrefix, encoded)
        && decode_file_id(encoded, file_id);
}

bool DownloadRequestParser::parse_shared_token(const HttpRequest& request,
    std::string& token) {
    return read_path_value(request.get_path(), kSharedPrefix, token)
        && is_lowercase_hex(token, kShareTokenHexLength);
}

} // namespace filelink
