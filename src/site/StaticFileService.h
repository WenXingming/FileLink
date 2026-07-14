#pragma once

#include <string>

namespace filelink {

// ====================================================================
// StaticFileService：校验 URI 路径后读取 web 根目录中的静态资源。
// ====================================================================
class StaticFileService {
public:
    explicit StaticFileService(std::string webRoot);

    std::string get_asset_content(const std::string& uriPath) const;

private:
    std::string webRoot_;
};

} // namespace filelink
