// ============================================================================
// Site 响应 View：构造健康检查、静态资源和站点错误响应。
// 集中维护状态码、JSON 格式和静态资源 MIME 类型。
// ============================================================================

#pragma once

#include <string>

class HttpResponse;

namespace filelink {

class SiteResponseView {
public:
    static HttpResponse health_check();
    static HttpResponse asset(const std::string& path, const std::string& content);
    static HttpResponse forbidden(const std::string& message);
    static HttpResponse not_found(const std::string& message);
};

} // namespace filelink
