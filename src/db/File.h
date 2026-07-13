#pragma once

#include <soci/soci.h>

#include <ctime>
#include <string>
#include <vector>

namespace filelink {
namespace db {

// =======================================================================
// File：用户可见的逻辑文件，引用内容寻址存储中的一个 Object。
// =======================================================================
struct File {
    std::string file_id;       // BINARY(16) mapped to std::string
    std::string owner_user_id; // BINARY(16) mapped to std::string
    std::string content_hash;  // BINARY(32) mapped to std::string
    std::string display_name;
    std::tm created_at{};
};

// =================================================================
// FileDao：创建逻辑文件，并按所有者读取其文件列表。
// =================================================================
class FileDao {
public:
    explicit FileDao(soci::session& sql) : sql_(sql) {}

    void create(const File& file);
    void find_by_owner(const std::string& owner_user_id, std::vector<File>& out_files);
    bool find_by_id_and_owner(const std::string& file_id, const std::string& owner_user_id,
        File& out_file);
    bool remove_by_id_and_owner(const std::string& file_id, const std::string& owner_user_id);

private:
    soci::session& sql_;
};

} // namespace db
} // namespace filelink
