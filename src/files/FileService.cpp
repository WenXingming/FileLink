#include "FileService.h"

#include <soci/connection-pool.h>

namespace filelink {

namespace {

class SociSessionLease {
public:
    explicit SociSessionLease(soci::connection_pool& pool) : pool_(pool), position_(pool.lease()) {}
    ~SociSessionLease() { pool_.give_back(position_); }

    soci::session& get() { return pool_.at(position_); }

private:
    soci::connection_pool& pool_;
    std::size_t position_;
};

} // namespace

void FileService::list_files(const std::string& owner_user_id, std::vector<db::File>& out_files) {
    SociSessionLease lease(pool_);
    db::FileDao(lease.get()).find_by_owner(owner_user_id, out_files);
}

} // namespace filelink
