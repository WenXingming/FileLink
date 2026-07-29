#pragma once

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
    explicit FileService(soci::connection_pool& pool)
        : pool_(pool) {}

    void list_files(const std::string& owner_user_id, std::vector<db::File>& out_files);
    bool find_file(const std::string& owner_user_id, const std::string& file_id, db::File& out_file);
    bool delete_file(const std::string& owner_user_id, const std::string& file_id);

private:
    soci::connection_pool& pool_;
};

} // namespace filelink
