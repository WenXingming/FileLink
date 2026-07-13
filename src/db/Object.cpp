#include "Object.h"

namespace filelink {
namespace db {

void ObjectDao::add_reference(const std::string& content_hash, uint64_t byte_size) {
    sql_ << "INSERT INTO objects (content_hash, byte_size, ref_count, state) "
            "VALUES (:hash, :size, 1, 'READY') "
            "ON DUPLICATE KEY UPDATE ref_count = ref_count + 1, state = 'READY'",
            soci::use(content_hash),
            soci::use(byte_size);
}

bool ObjectDao::remove_reference(const std::string& content_hash) {
    soci::statement statement = (sql_.prepare
        << "UPDATE objects "
           "SET state = CASE WHEN ref_count = 1 THEN 'PENDING_DELETE' ELSE state END, "
               "ref_count = ref_count - 1 "
           "WHERE content_hash = :hash AND ref_count > 0",
        soci::use(content_hash));
    statement.execute(false);
    return statement.get_affected_rows() == 1;
}

bool ObjectDao::find(const std::string& content_hash, Object& out_object) {
    soci::indicator result_ind;

    sql_ << "SELECT content_hash, byte_size, ref_count, state "
            "FROM objects WHERE content_hash = :hash",
            soci::into(out_object.content_hash, result_ind),
            soci::into(out_object.byte_size),
            soci::into(out_object.ref_count),
            soci::into(out_object.state),
            soci::use(content_hash);

    return result_ind == soci::i_ok;
}

} // namespace db
} // namespace filelink
