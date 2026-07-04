#pragma once

#include <string>

namespace filelink {

class StaticFileService {
public:
    explicit StaticFileService(std::string webRoot);

    // 提取静态文件内容。如有异常（如越权访问、文件不存在）会抛出 std::runtime_error 或 std::invalid_argument
    std::string get_asset_content(const std::string& uriPath) const;

private:
    std::string webRoot_;
};

} // namespace filelink
