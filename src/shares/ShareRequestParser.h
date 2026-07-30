// ============================================================================
// 分享请求解析器：解析分享管理路径和分享有效期。
// 只判断 HTTP 输入格式，不鉴权、不查询分享，也不构造响应。
// ============================================================================

#pragma once

#include <ctime>
#include <string>

class HttpRequest;

namespace filelink {

class ShareRequestParser {
public:
    static bool parse_collection_file_id(const HttpRequest& request, std::string& file_id);
    static bool parse_share_ids(const HttpRequest& request, std::string& file_id, std::string& share_id);
    static bool parse_expiry(const HttpRequest& request, std::time_t& expiry);
};

} // namespace filelink
