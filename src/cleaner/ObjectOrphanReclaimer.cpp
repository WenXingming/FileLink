#include "ObjectOrphanReclaimer.h"

#include "db/Object.h"
#include "db/SociSessionLease.h"

#include <algorithm>
#include <cerrno>
#include <dirent.h>
#include <system_error>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace filelink {

namespace {

bool is_lower_hex(const std::string& value) {
    return std::all_of(value.begin(), value.end(), [](char character) {
        return (character >= '0' && character <= '9')
            || (character >= 'a' && character <= 'f');
    });
}

std::vector<std::string> list_directory(const std::string& path) {
    DIR* directory = ::opendir(path.c_str());
    if (directory == nullptr) {
        if (errno == ENOENT) {
            return {};
        }
        throw std::system_error(errno, std::generic_category(), path);
    }

    std::vector<std::string> entries;
    errno = 0;
    while (dirent* entry = ::readdir(directory)) {
        const std::string name(entry->d_name);
        if (name != "." && name != "..") {
            entries.push_back(name);
        }
    }
    const int read_error = errno;
    (void)::closedir(directory);
    if (read_error != 0) {
        throw std::system_error(read_error, std::generic_category(), path);
    }

    std::sort(entries.begin(), entries.end());
    return entries;
}

bool is_regular_file(const std::string& path) {
    struct stat info;
    if (::lstat(path.c_str(), &info) == 0) {
        return S_ISREG(info.st_mode);
    }
    if (errno == ENOENT) {
        return false;
    }
    throw std::system_error(errno, std::generic_category(), path);
}

std::string decode_hash(const std::string& hash_hex) {
    std::string hash;
    hash.reserve(32);
    for (std::size_t index = 0; index < hash_hex.size(); index += 2) {
        const auto decode = [](char character) {
            return character <= '9' ? character - '0' : character - 'a' + 10;
        };
        hash.push_back(static_cast<char>(decode(hash_hex[index]) * 16
            + decode(hash_hex[index + 1])));
    }
    return hash;
}

} // namespace

ObjectOrphanReclaimer::ObjectOrphanReclaimer(soci::connection_pool& pool,
    std::string storage_root)
    : pool_(pool), storage_root_(std::move(storage_root)) {
}

std::vector<std::string> ObjectOrphanReclaimer::find_orphaned_object_paths() {
    db::SociSessionLease lease(pool_);
    db::ObjectDao objects(lease.get());
    std::vector<std::string> orphaned_paths;
    const std::string objects_root = storage_root_ + "/objects";

    for (const std::string& first_level : list_directory(objects_root)) {
        if (first_level.size() != 2 || !is_lower_hex(first_level)) {
            continue;
        }
        const std::string first_path = objects_root + "/" + first_level;

        for (const std::string& second_level : list_directory(first_path)) {
            if (second_level.size() != 2 || !is_lower_hex(second_level)) {
                continue;
            }
            const std::string second_path = first_path + "/" + second_level;

            for (const std::string& hash_hex : list_directory(second_path)) {
                if (hash_hex.size() != 64 || !is_lower_hex(hash_hex)
                    || hash_hex.substr(0, 2) != first_level
                    || hash_hex.substr(2, 2) != second_level) {
                    continue;
                }

                const std::string object_path = second_path + "/" + hash_hex;
                if (!is_regular_file(object_path)) {
                    continue;
                }

                db::Object object;
                if (!objects.find(decode_hash(hash_hex), object)) {
                    orphaned_paths.push_back(object_path);
                }
            }
        }
    }
    return orphaned_paths;
}

int ObjectOrphanReclaimer::reclaim_orphaned_objects() {
    const std::vector<std::string> paths = find_orphaned_object_paths();

    db::SociSessionLease lease(pool_);
    db::ObjectDao objects(lease.get());
    int reclaimed_count = 0;
    for (const std::string& path : paths) {
        const std::string hash_hex = path.substr(path.size() - 64);
        db::Object object;
        if (objects.find(decode_hash(hash_hex), object)) {
            continue;
        }
        if (::unlink(path.c_str()) == 0) {
            ++reclaimed_count;
        }
    }
    return reclaimed_count;
}

} // namespace filelink
