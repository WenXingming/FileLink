// ============================================================================
// 文件响应 View：把文件用例结果表示为状态码和 JSON。
// 文件字节下载的共享 X-Accel-Redirect 响应暂由 ApiResponseView 维护。
// ============================================================================

#pragma once

#include "database/File.h"

#include <string>
#include <vector>

class HttpResponse;

namespace filelink {

class FileResponseView {
public:
    static HttpResponse file_list(const std::vector<db::File>& files);
    static HttpResponse deleted();
    static HttpResponse unauthorized();
    static HttpResponse not_found();
    static HttpResponse server_error(const std::string& message);
};

} // namespace filelink
