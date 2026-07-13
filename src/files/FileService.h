#pragma once

#include "db/File.h"

#include <vector>

namespace soci {
class connection_pool;
}

namespace filelink {

// ========================================================
// FileService：读取用户拥有的逻辑文件。
// ========================================================
class FileService {
public:
    explicit FileService(soci::connection_pool& pool) : pool_(pool) {}

    void list_files(const std::string& owner_user_id, std::vector<db::File>& out_files);

private:
    soci::connection_pool& pool_;
};

} // namespace filelink
