#pragma once

#include "ObjectStore.h"
#include <string>
#include <atomic>

namespace filelink {

class DownloadService {
public:
    DownloadService(ObjectStore store);

    // 禁止拷贝，保持一致性
    DownloadService(const DownloadService&) = delete;
    DownloadService& operator=(const DownloadService&) = delete;
    
    std::string get_object_path(const std::string& contentHash) const;
    std::string get_object_content(const std::string& contentHash) const;

private:
    ObjectStore store_;
};

} // namespace filelink
