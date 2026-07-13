#pragma once

#include <string>
#include <vector>

namespace soci {
class connection_pool;
}

namespace filelink {

// ========================================================================
// ObjectOrphanScanner：找出磁盘上没有 objects 记录的正式对象文件。
// ========================================================================
class ObjectOrphanScanner {
public:
    ObjectOrphanScanner(soci::connection_pool& pool, std::string storage_root);

    std::vector<std::string> find_orphaned_object_paths();

private:
    soci::connection_pool& pool_;
    std::string storage_root_;
};

} // namespace filelink
