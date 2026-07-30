// ============================================================================
// 文件请求解析器：从 Files 路由路径中解析二进制文件 ID。
// 只判断 HTTP 输入格式，不查询文件、鉴权或构造响应。
// ============================================================================

#pragma once

#include <string>

class HttpRequest;

namespace filelink {

class FileRequestParser {
public:
    static bool parse_file_id(const HttpRequest& request, std::string& file_id);
};

} // namespace filelink
