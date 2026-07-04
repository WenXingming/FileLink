#pragma once

#include "ObjectStore.h"
#include <string>
#include <atomic>

namespace filelink {

struct UploadResult {
    std::string hash;
    std::string status;
    std::string extension;
};

class ObjectService {
public:
    ObjectService(ObjectStore store, std::string storageRoot);

    // 禁止拷贝，因为持有 std::atomic
    ObjectService(const ObjectService&) = delete;
    ObjectService& operator=(const ObjectService&) = delete;

    UploadResult process_upload(const std::string& fileBody, const std::string& fileName);
    
    std::string get_object_path(const std::string& contentHash) const;
    std::string get_object_content(const std::string& contentHash) const;

private:
    ObjectStore store_;
    std::string storageRoot_;
    std::atomic<uint64_t> reqCounter_{0};
};

} // namespace filelink
