// ============================================================================
// 文件业务服务实现：租用数据库连接执行所有权查询，并维护删除事务边界。
// 删除逻辑文件和递减对象引用必须在同一事务中完成。
// ============================================================================

#include "FileService.h"

#include "database/Object.h"
#include "database/SociSessionLease.h"

#include <stdexcept>

namespace filelink {

FileService::FileService(soci::connection_pool& pool) : pool_(pool) {}

std::vector<db::File> FileService::list_files(const std::string& owner_user_id) {
    db::SociSessionLease lease(pool_);
    std::vector<db::File> files;
    db::FileDao(lease.get()).find_by_owner(owner_user_id, files);
    return files;
}

bool FileService::find_file(const std::string& owner_user_id, const std::string& file_id,
    db::File& out_file) {
    db::SociSessionLease lease(pool_);
    return db::FileDao(lease.get()).find_by_id_and_owner(file_id, owner_user_id, out_file);
}

bool FileService::delete_file(const std::string& owner_user_id, const std::string& file_id) {
    db::SociSessionLease lease(pool_);
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

} // namespace filelink
