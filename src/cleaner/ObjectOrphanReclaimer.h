#pragma once

#include <string>

namespace soci {
class connection_pool;
}

namespace filelink {

// ======================================================================
// ObjectOrphanReclaimer：离线删除没有 objects 记录的孤儿对象文件。
// ======================================================================
class ObjectOrphanReclaimer {
public:
    ObjectOrphanReclaimer(soci::connection_pool& pool, std::string storage_root);

    int reclaim_orphaned_objects();

private:
    soci::connection_pool& pool_;
    std::string storage_root_;
};

} // namespace filelink
