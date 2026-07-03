#include "LocalObjectStore.h"

#include <algorithm>
#include <cerrno>
#include <stdexcept>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace {

std::string join_path(const std::string& parent, const std::string& child) {
    return parent.back() == '/' ? parent + child : parent + "/" + child;
}

bool is_valid_hash(const std::string& hash) {
    return hash.size() == 64
        && std::all_of(hash.begin(), hash.end(), [](char value) {
            return (value >= '0' && value <= '9')
                || (value >= 'a' && value <= 'f');
        });
}

void ensure_directory(const std::string& path) {
    struct stat info;
    if (::stat(path.c_str(), &info) == 0) {
        if (S_ISDIR(info.st_mode)) {
            return;
        }
        throw std::system_error(ENOTDIR, std::generic_category(), path);
    }

    const int statError = errno;
    if (statError != ENOENT) {
        throw std::system_error(statError, std::generic_category(), path);
    }

    if (::mkdir(path.c_str(), 0755) == 0) {
        return;
    }

    const int mkdirError = errno;
    if (mkdirError == EEXIST
        && ::stat(path.c_str(), &info) == 0
        && S_ISDIR(info.st_mode)) {
        return;
    }
    throw std::system_error(mkdirError, std::generic_category(), path);
}

} // namespace

namespace filelink {

LocalObjectStore::LocalObjectStore(std::string storageRoot)
    : storageRoot_(std::move(storageRoot)) {
    if (storageRoot_.empty()) {
        throw std::invalid_argument("存储根目录不能为空");
    }
}

CommitResult LocalObjectStore::commit(
    const std::string& tempPath,
    const std::string& contentHash) const {
    if (!is_valid_hash(contentHash)) {
        throw std::invalid_argument("内容摘要必须是 64 位小写十六进制字符串");
    }

    const std::string objectsDir = join_path(storageRoot_, "objects");
    const std::string firstLevelDir = join_path(objectsDir, contentHash.substr(0, 2));
    const std::string secondLevelDir = join_path(firstLevelDir, contentHash.substr(2, 2));
    const std::string objectPath = join_path(secondLevelDir, contentHash);

    ensure_directory(storageRoot_);
    ensure_directory(objectsDir);
    ensure_directory(firstLevelDir);
    ensure_directory(secondLevelDir);

    // link 是不覆盖的原子发布点，unlink 只负责清理临时目录项。
    if (::link(tempPath.c_str(), objectPath.c_str()) == 0) {
        (void)::unlink(tempPath.c_str());
        return CommitResult{CommitStatus::Created, objectPath};
    }

    const int linkError = errno;
    if (linkError == EEXIST) {
        (void)::unlink(tempPath.c_str());
        return CommitResult{CommitStatus::Reused, objectPath};
    }

    throw std::system_error(linkError, std::generic_category(), tempPath);
}

std::string LocalObjectStore::getObjectPath(const std::string& contentHash) const {
    if (!is_valid_hash(contentHash)) {
        throw std::invalid_argument("内容摘要必须是 64 位小写十六进制字符串");
    }

    const std::string objectsDir = join_path(storageRoot_, "objects");
    const std::string firstLevelDir = join_path(objectsDir, contentHash.substr(0, 2));
    const std::string secondLevelDir = join_path(firstLevelDir, contentHash.substr(2, 2));
    return join_path(secondLevelDir, contentHash);
}

} // namespace filelink
