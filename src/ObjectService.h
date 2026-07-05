#pragma once

#include "ObjectStore.h"
#include <string>
#include <atomic>

namespace filelink {

class ObjectService {
public:
    ObjectService(ObjectStore store);

    // 禁止拷贝，保持一致性
    ObjectService(const ObjectService&) = delete;
    ObjectService& operator=(const ObjectService&) = delete;
    
    std::string get_object_path(const std::string& contentHash) const;
    std::string get_object_content(const std::string& contentHash) const;

private:
    ObjectStore store_;
};

} // namespace filelink
