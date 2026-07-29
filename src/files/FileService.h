// ============================================================================
// 文件业务服务：查询当前用户拥有的逻辑文件，并以事务删除文件引用。
// 不解析 HTTP，也不构造响应或读取物理文件内容。
// ============================================================================

#pragma once

#include "database/File.h"

#include <string>
#include <vector>

namespace soci {
class connection_pool;
}

namespace filelink {

class FileService {
public:
    explicit FileService(soci::connection_pool& pool);

    std::vector<db::File> list_files(const std::string& owner_user_id);
    bool find_file(const std::string& owner_user_id, const std::string& file_id, db::File& out_file);
    bool delete_file(const std::string& owner_user_id, const std::string& file_id);

private:
    soci::connection_pool& pool_;
};

} // namespace filelink
