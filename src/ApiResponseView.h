#pragma once

#include "tudou/http/HttpResponse.h"
#include <string>

namespace filelink {

namespace db {
struct UploadSession;
}

class ApiResponseView {
public:
    // 构建标准的 JSON 返回格式，并自动附带 Content-Type 头
    static HttpResponse json(int statusCode, const std::string& jsonBody);

    // 针对具体业务的 View 渲染器
    static HttpResponse health_check();
    static HttpResponse error(int statusCode, const std::string& message);
    
    // 专门构建二进制/文本文件的 HTTP 响应
    static HttpResponse file(const std::string& content, const std::string& extension);

    // Tus 协议的 View 渲染器
    static HttpResponse tus_options();
    static HttpResponse tus_head(uint64_t offset, uint64_t length);
    static HttpResponse tus_created(const std::string& uploadIdHex, const std::string& host);
    static HttpResponse tus_patched(uint64_t offset);
    static HttpResponse tus_error(int statusCode, const std::string& statusMessage, const std::string& message);
    static HttpResponse tus_session_status(const db::UploadSession& session);
    
private:
    // 提取的静态方法：推断 MIME 类型
    static std::string infer_mime_type(const std::string& ext);
    // 基础的 JSON 字符串转义工具，防止双引号引起的 JSON 注入漏洞
    static std::string escape_json(const std::string& input);
};

} // namespace filelink
