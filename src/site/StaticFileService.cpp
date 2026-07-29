// ============================================================================
// 静态文件服务实现：阻止目录穿越，只把普通文件内容读入内存。
// 路径到 HTTP 状态码的映射由 SiteRouter 决定。
// ============================================================================

#include "StaticFileService.h"

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <sys/stat.h>
#include <utility>

namespace filelink {

StaticFileService::StaticFileService(std::string web_root)
    : webRoot_(std::move(web_root)) {}

std::string StaticFileService::read_asset(const std::string& uri_path) const {
    if (uri_path.find("..") != std::string::npos) {
        throw std::invalid_argument("Forbidden: Directory traversal detected");
    }

    const std::string file_path = webRoot_ + uri_path;
    struct stat info {};
    if (::stat(file_path.c_str(), &info) != 0 || !S_ISREG(info.st_mode)) {
        throw std::runtime_error("Not Found");
    }

    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open file");
    }

    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

} // namespace filelink
