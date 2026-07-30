// ============================================================================
// 分享响应 View：把分享用例结果表示为 JSON、状态码和空响应。
// 不查询分享、不验证权限，也不处理文件下载。
// ============================================================================

#pragma once

#include <string>
#include <vector>

class HttpResponse;

namespace filelink {

struct CreatedShare;

namespace db {
struct Share;
}

class ShareResponseView {
public:
    static HttpResponse created(const CreatedShare& share);
    static HttpResponse share_list(const std::vector<db::Share>& shares);
    static HttpResponse revoked();
    static HttpResponse invalid_expiry();
    static HttpResponse unauthorized();
    static HttpResponse not_found();
    static HttpResponse server_error(const std::string& message);
};

} // namespace filelink
