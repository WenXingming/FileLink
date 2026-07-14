#include "ObjectReclaimer.h"

#include "database/Object.h"
#include "database/SociSessionLease.h"

#include <cerrno>
#include <iomanip>
#include <sstream>
#include <unistd.h>
#include <vector>

namespace filelink {

namespace {

std::string hex_encode(const std::string& bytes) {
    std::stringstream stream;
    stream << std::hex << std::setfill('0');
    for (unsigned char byte : bytes) {
        stream << std::setw(2) << static_cast<int>(byte);
    }
    return stream.str();
}

} // namespace

int ObjectReclaimer::reclaim_pending_objects() {
    db::SociSessionLease lease(pool_);
    db::ObjectDao objects(lease.get());
    std::vector<db::Object> candidates;
    objects.find_reclaimable(candidates);

    int reclaimed_count = 0;
    for (const db::Object& object : candidates) {
        if (object.state == "PENDING_DELETE" && !objects.claim_pending_delete(object.content_hash)) {
            continue;
        }

        const std::string object_path = store_.get_object_path(hex_encode(object.content_hash));
        if (::unlink(object_path.c_str()) != 0 && errno != ENOENT) {
            objects.return_to_pending_delete(object.content_hash);
            continue;
        }
        if (objects.remove_reclaiming(object.content_hash)) {
            ++reclaimed_count;
        }
    }
    return reclaimed_count;
}

} // namespace filelink
