#include "ObjectStore.h"

#include <algorithm>
#include <cerrno>
#include <fcntl.h>
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

std::string parent_path(const std::string& path) {
    const std::string::size_type lastCharacter = path.find_last_not_of('/');
    if (lastCharacter == std::string::npos) {
        return "/";
    }

    const std::string::size_type separator = path.rfind('/', lastCharacter);
    if (separator == std::string::npos) {
        return ".";
    }
    return separator == 0 ? "/" : path.substr(0, separator);
}

void sync_file(const std::string& path) {
    const int fd = ::open(path.c_str(), O_RDWR | O_CLOEXEC);
    if (fd == -1) {
        throw std::system_error(errno, std::generic_category(), path);
    }

    if (::fdatasync(fd) == -1) {
        const int syncError = errno;
        (void)::close(fd);
        throw std::system_error(syncError, std::generic_category(), path);
    }
    if (::close(fd) == -1) {
        throw std::system_error(errno, std::generic_category(), path);
    }
}

void sync_directory(const std::string& path) {
    const int fd = ::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd == -1) {
        throw std::system_error(errno, std::generic_category(), path);
    }

    if (::fsync(fd) == -1) {
        const int syncError = errno;
        (void)::close(fd);
        throw std::system_error(syncError, std::generic_category(), path);
    }
    if (::close(fd) == -1) {
        throw std::system_error(errno, std::generic_category(), path);
    }
}

void ensure_directory(const std::string& path, const std::string& parent) {
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
        sync_directory(parent);
        return;
    }

    const int mkdirError = errno;
    if (mkdirError == EEXIST
        && ::stat(path.c_str(), &info) == 0
        && S_ISDIR(info.st_mode)) {
        // 另一个线程刚创建目录时，也同步父目录以完成持久化。
        sync_directory(parent);
        return;
    }
    throw std::system_error(mkdirError, std::generic_category(), path);
}

} // namespace

namespace filelink {

ObjectStore::ObjectStore(std::string storageRoot)
    : storageRoot_(std::move(storageRoot)) {
    if (storageRoot_.empty()) {
        throw std::invalid_argument("存储根目录不能为空");
    }
}

CommitResult ObjectStore::commit(const std::string& tempPath, const std::string& contentHash) const {
    if (!is_valid_hash(contentHash)) {
        throw std::invalid_argument("内容摘要必须是 64 位小写十六进制字符串");
    }

    const std::string objectsDir = join_path(storageRoot_, "objects");
    const std::string firstLevelDir = join_path(objectsDir, contentHash.substr(0, 2));
    const std::string secondLevelDir = join_path(firstLevelDir, contentHash.substr(2, 2));
    const std::string objectPath = join_path(secondLevelDir, contentHash);

    ensure_directory(storageRoot_, parent_path(storageRoot_));
    ensure_directory(objectsDir, storageRoot_);
    ensure_directory(firstLevelDir, objectsDir);
    ensure_directory(secondLevelDir, firstLevelDir);

    // 先持久化内容，再发布正式名字，避免目录项指向未落盘的数据。
    sync_file(tempPath);

    if (::link(tempPath.c_str(), objectPath.c_str()) == 0) {
        // link 只保证命名空间原子性，目录 fsync 才保证新名字抗掉电。
        sync_directory(secondLevelDir);
        (void)::unlink(tempPath.c_str());
        return CommitResult{ CommitStatus::Created, objectPath };
    }

    const int linkError = errno;
    if (linkError == EEXIST) {
        (void)::unlink(tempPath.c_str());
        return CommitResult{ CommitStatus::Reused, objectPath };
    }

    throw std::system_error(linkError, std::generic_category(), tempPath);
}

std::string ObjectStore::get_object_key(const std::string& contentHash) {
    if (!is_valid_hash(contentHash)) {
        throw std::invalid_argument("内容摘要必须是 64 位小写十六进制字符串");
    }

    const std::string firstLevelDir = contentHash.substr(0, 2);
    const std::string secondLevelDir = join_path(firstLevelDir, contentHash.substr(2, 2));
    return join_path(secondLevelDir, contentHash);
}

std::string ObjectStore::get_object_path(const std::string& contentHash) const {
    return join_path(join_path(storageRoot_, "objects"), ObjectStore::get_object_key(contentHash));
}

} // namespace filelink
