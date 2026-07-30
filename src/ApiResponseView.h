// ============================================================================
// Tus 上传协议响应 View：构造上传控制接口的响应。
// 站点页面与健康检查响应由 site/SiteResponseView 负责。
// ============================================================================

#pragma once

#include "tudou/http/HttpResponse.h"
#include <string>

namespace filelink {

namespace db {
struct UploadSession;
}

class ApiResponseView {
public:
    static HttpResponse tus_options();
    static HttpResponse tus_head(uint64_t offset, uint64_t length);
    static HttpResponse tus_created(const std::string& uploadIdHex, const std::string& host);
    static HttpResponse tus_patched(uint64_t offset);
    static HttpResponse tus_error(int statusCode, const std::string& statusMessage, const std::string& message);
    static HttpResponse tus_session_status(const db::UploadSession& session);
};

} // namespace filelink
