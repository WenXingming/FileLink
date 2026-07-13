#include "ObjectOrphanReclaimer.h"

#include "ObjectOrphanScanner.h"
#include "db/Object.h"
#include "db/SociSessionLease.h"

#include <unistd.h>
#include <utility>
#include <vector>

namespace filelink {

namespace {

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

int ObjectOrphanReclaimer::reclaim_orphaned_objects() {
    ObjectOrphanScanner scanner(pool_, storage_root_);
    const std::vector<std::string> paths = scanner.find_orphaned_object_paths();

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
