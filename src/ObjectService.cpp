#include "ObjectService.h"
#include "StreamUploader.h"
#include <chrono>
#include <fstream>
#include <sys/stat.h>
#include <stdexcept>

namespace filelink {

ObjectService::ObjectService(ObjectStore store)
    : store_(std::move(store)) {
}

std::string ObjectService::get_object_path(const std::string& contentHash) const {
    return store_.get_object_path(contentHash);
}

std::string ObjectService::get_object_content(const std::string& contentHash) const {
    std::string objectPath = store_.get_object_path(contentHash);

    struct stat info;
    if (::stat(objectPath.c_str(), &info) != 0) {
        throw std::invalid_argument("Object Not Found");
    }

    std::ifstream ifs(objectPath, std::ios::binary);
    if (!ifs) {
        throw std::runtime_error("Failed to open object");
    }

    return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

} // namespace filelink
