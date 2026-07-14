#pragma once

#include "storage/ObjectStore.h"

#include <utility>

namespace soci {
class connection_pool;
}

namespace filelink {

// =====================================================================
// ObjectReclaimer：离线删除没有逻辑文件引用的物理对象及其记录（零引用对象清理）。
// =====================================================================
class ObjectReclaimer {
public:
    ObjectReclaimer(soci::connection_pool& pool, ObjectStore store)
        : pool_(pool), store_(std::move(store)) {}

    int reclaim_pending_objects();

private:
    soci::connection_pool& pool_;
    ObjectStore store_;
};

} // namespace filelink
