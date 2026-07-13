#include "File.h"

namespace filelink {
namespace db {

void FileDao::create(const File& file) {
    sql_ << "INSERT INTO files (file_id, owner_user_id, content_hash, display_name) "
            "VALUES (:file_id, :owner_user_id, :content_hash, :display_name)",
            soci::use(file.file_id),
            soci::use(file.owner_user_id),
            soci::use(file.content_hash),
            soci::use(file.display_name);
}

void FileDao::find_by_owner(const std::string& owner_user_id, std::vector<File>& out_files) {
    soci::rowset<soci::row> rows = (sql_.prepare
        << "SELECT file_id, owner_user_id, content_hash, display_name, created_at "
           "FROM files WHERE owner_user_id = :owner_user_id "
           "ORDER BY created_at DESC, file_id DESC",
        soci::use(owner_user_id));

    out_files.clear();
    for (const soci::row& row : rows) {
        out_files.push_back(File{
            row.get<std::string>(0),
            row.get<std::string>(1),
            row.get<std::string>(2),
            row.get<std::string>(3),
            row.get<std::tm>(4)
        });
    }
}

} // namespace db
} // namespace filelink
