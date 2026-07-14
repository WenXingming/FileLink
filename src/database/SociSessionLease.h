#pragma once

#include <soci/connection-pool.h>

#include <cstddef>

namespace filelink {
namespace db {

// =====================================================================
// SociSessionLease：基于 RAII 实现了对连接池中连接会话的自动化租借和归还
// =====================================================================
class SociSessionLease {
public:
    explicit SociSessionLease(soci::connection_pool& pool)
        : pool_(pool)
        , position_(pool.lease()) {
    }
    ~SociSessionLease() {
        pool_.give_back(position_);
    }

    SociSessionLease(const SociSessionLease&) = delete;
    SociSessionLease& operator=(const SociSessionLease&) = delete;

    soci::session& get() {
        return pool_.at(position_);
    }

private:
    soci::connection_pool& pool_;
    std::size_t position_;
};

} // namespace db
} // namespace filelink
