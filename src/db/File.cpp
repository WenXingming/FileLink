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

bool FileDao::find_by_id_and_owner(const std::string& file_id, const std::string& owner_user_id,
    File& out_file) {
    soci::indicator result_ind;

    sql_ << "SELECT file_id, owner_user_id, content_hash, display_name, created_at "
            "FROM files WHERE file_id = :file_id AND owner_user_id = :owner_user_id",
            soci::into(out_file.file_id, result_ind),
            soci::into(out_file.owner_user_id),
            soci::into(out_file.content_hash),
            soci::into(out_file.display_name),
            soci::into(out_file.created_at),
            soci::use(file_id),
            soci::use(owner_user_id);

    return result_ind == soci::i_ok;
}

bool FileDao::remove_by_id_and_owner(const std::string& file_id, const std::string& owner_user_id) {
    soci::statement statement = (sql_.prepare
        << "DELETE FROM files WHERE file_id = :file_id AND owner_user_id = :owner_user_id",
        soci::use(file_id),
        soci::use(owner_user_id));
    statement.execute(false);
    return statement.get_affected_rows() == 1;
}

} // namespace db
} // namespace filelink
