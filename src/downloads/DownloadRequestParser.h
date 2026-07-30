// ============================================================================
// 下载请求解析器：解析私有文件 ID 和公开分享 Token。
// 只判断 URL 格式，不鉴权、不查询文件，也不构造响应。
// ============================================================================

#pragma once

#include <string>

class HttpRequest;

namespace filelink {

class DownloadRequestParser {
public:
    static bool parse_private_file_id(const HttpRequest& request, std::string& file_id);
    static bool parse_shared_token(const HttpRequest& request, std::string& token);
};

} // namespace filelink
