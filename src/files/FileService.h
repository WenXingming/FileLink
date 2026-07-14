#pragma once

#include "storage/ObjectStore.h"
#include "database/File.h"

#include <vector>

namespace soci {
class connection_pool;
}

namespace filelink {

// ========================================================
// FileService：读取和删除用户拥有的逻辑文件。
// ========================================================
class FileService {
public:
    FileService(soci::connection_pool& pool, ObjectStore store)
        : pool_(pool), store_(std::move(store)) {}

    void list_files(const std::string& owner_user_id, std::vector<db::File>& out_files);
    bool find_file(const std::string& owner_user_id, const std::string& file_id, db::File& out_file);
    bool delete_file(const std::string& owner_user_id, const std::string& file_id);
    std::string object_path(const db::File& file) const;

private:
    soci::connection_pool& pool_;
    ObjectStore store_;
};

} // namespace filelink
