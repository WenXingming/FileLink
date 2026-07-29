// ============================================================================
// 静态文件服务：校验资源路径并读取 web 根目录中的普通文件。
// 不解释 MIME 类型，也不构造 HTTP 响应。
// ============================================================================

#pragma once

#include <string>

namespace filelink {

class StaticFileService {
public:
    explicit StaticFileService(std::string web_root);

    std::string read_asset(const std::string& uri_path) const;

private:
    std::string webRoot_;
};

} // namespace filelink
