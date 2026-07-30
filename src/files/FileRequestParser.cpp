// ============================================================================
// 文件请求解析器实现：校验固定路由形状，并把 32 位小写十六进制 ID 解码为 16 字节。
// 路径不匹配或 ID 非法时统一返回 false，由 Router 决定 HTTP 语义。
// ============================================================================

#include "FileRequestParser.h"

#include "tudou/http/HttpRequest.h"

namespace filelink {

namespace {

const std::string kFilesPrefix = "/files/";
const std::size_t kFileIdHexLength = 32;

int hex_digit(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    return -1;
}

bool decode_file_id(const std::string& encoded, std::string& file_id) {
    if (encoded.size() != kFileIdHexLength) {
        return false;
    }

    file_id.clear();
    file_id.reserve(kFileIdHexLength / 2);
    for (std::size_t index = 0; index < encoded.size(); index += 2) {
        const int high = hex_digit(encoded[index]);
        const int low = hex_digit(encoded[index + 1]);
        if (high < 0 || low < 0) {
            return false;
        }
        file_id.push_back(static_cast<char>((high << 4) | low));
    }
    return true;
}

bool parse_file_path(const std::string& path, std::string& file_id) {
    if (path.size() != kFilesPrefix.size() + kFileIdHexLength
        || path.compare(0, kFilesPrefix.size(), kFilesPrefix) != 0) {
        return false;
    }
    return decode_file_id(path.substr(kFilesPrefix.size(), kFileIdHexLength), file_id);
}

} // namespace

bool FileRequestParser::parse_file_id(const HttpRequest& request, std::string& file_id) {
    return parse_file_path(request.get_path(), file_id);
}

} // namespace filelink
