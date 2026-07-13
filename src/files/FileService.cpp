#include "FileService.h"

#include "db/Object.h"

#include <soci/connection-pool.h>

#include <iomanip>
#include <sstream>
#include <stdexcept>

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

bool FileService::find_file(const std::string& owner_user_id, const std::string& file_id,
    db::File& out_file) {
    SociSessionLease lease(pool_);
    return db::FileDao(lease.get()).find_by_id_and_owner(file_id, owner_user_id, out_file);
}

bool FileService::delete_file(const std::string& owner_user_id, const std::string& file_id) {
    SociSessionLease lease(pool_);
    soci::session& sql = lease.get();
    soci::transaction transaction(sql);

    db::File file;
    db::FileDao file_dao(sql);
    if (!file_dao.find_by_id_and_owner(file_id, owner_user_id, file)) {
        return false;
    }
    if (!file_dao.remove_by_id_and_owner(file_id, owner_user_id)) {
        return false;
    }
    if (!db::ObjectDao(sql).remove_reference(file.content_hash)) {
        throw std::runtime_error("logical file references a missing object");
    }

    transaction.commit();
    return true;
}

std::string FileService::object_path(const db::File& file) const {
    std::stringstream stream;
    stream << std::hex << std::setfill('0');
    for (unsigned char value : file.content_hash) {
        stream << std::setw(2) << static_cast<int>(value);
    }
    return store_.get_object_path(stream.str());
}

} // namespace filelink
