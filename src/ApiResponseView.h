#pragma once

#include "tudou/http/HttpResponse.h"
#include "ObjectService.h"
#include <string>

namespace filelink {

class ApiResponseView {
public:
    // 构建标准的 JSON 返回格式，并自动附带 Content-Type 头
    static HttpResponse json(int statusCode, const std::string& jsonBody);

    // 针对具体业务的 View 渲染器
    static HttpResponse health_check();
    static HttpResponse upload_success(const UploadResult& result, const std::string& host);
    static HttpResponse error(int statusCode, const std::string& message);
    
    // 专门构建二进制/文本文件的 HTTP 响应
    static HttpResponse file(const std::string& content, const std::string& extension);
    
private:
    // 提取的静态方法：推断 MIME 类型
    static std::string infer_mime_type(const std::string& ext);
    // 基础的 JSON 字符串转义工具，防止双引号引起的 JSON 注入漏洞
    static std::string escape_json(const std::string& input);
};

} // namespace filelink
