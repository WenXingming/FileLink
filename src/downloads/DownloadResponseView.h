// ============================================================================
// 下载响应 View：把下载目标表示为 Nginx 内部重定向或 JSON 错误响应。
// 不查询文件、不验证权限，也不读取文件内容。
// ============================================================================

#pragma once

#include <string>

class HttpResponse;

namespace filelink {

struct DownloadTarget;

class DownloadResponseView {
public:
    static HttpResponse file(const DownloadTarget& target);
    static HttpResponse unauthorized();
    static HttpResponse not_found();
    static HttpResponse server_error(const std::string& message);
};

} // namespace filelink
