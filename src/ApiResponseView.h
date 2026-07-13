#pragma once

#include "tudou/http/HttpResponse.h"
#include <string>

namespace filelink {

namespace db {
struct UploadSession;
}

// =====================================================================
// ApiResponseView：集中构造健康检查、静态资源和 Tus 协议响应。
// =====================================================================
class ApiResponseView {
public:
    static HttpResponse json(int statusCode, const std::string& jsonBody);

    static HttpResponse health_check();
    static HttpResponse error(int statusCode, const std::string& message);
    static HttpResponse file(const std::string& content, const std::string& extension);

    static HttpResponse tus_options();
    static HttpResponse tus_head(uint64_t offset, uint64_t length);
    static HttpResponse tus_created(const std::string& uploadIdHex, const std::string& host);
    static HttpResponse tus_patched(uint64_t offset);
    static HttpResponse tus_error(int statusCode, const std::string& statusMessage, const std::string& message);
    static HttpResponse tus_session_status(const db::UploadSession& session);
    
private:
    static std::string infer_mime_type(const std::string& ext);
    static std::string escape_json(const std::string& input);
};

} // namespace filelink
