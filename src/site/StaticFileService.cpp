#include "StaticFileService.h"

#include <fstream>
#include <stdexcept>
#include <sys/stat.h>

namespace filelink {

StaticFileService::StaticFileService(std::string webRoot)
    : webRoot_(std::move(webRoot)) {}

std::string StaticFileService::get_asset_content(const std::string& uriPath) const {
    // 1. 安全检查：防路径穿越
    if (uriPath.find("..") != std::string::npos) {
        throw std::invalid_argument("Forbidden: Directory traversal detected");
    }

    std::string filePath = webRoot_ + uriPath;
    
    // 2. 存在性及常规文件检查
    struct stat info;
    if (::stat(filePath.c_str(), &info) != 0 || !S_ISREG(info.st_mode)) {
        throw std::runtime_error("Not Found");
    }

    // 3. 读取文件内容
    std::ifstream ifs(filePath, std::ios::binary);
    if (!ifs) {
        throw std::runtime_error("Failed to open file");
    }
    
    return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

} // namespace filelink
