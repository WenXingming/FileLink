#include "Object.h"

namespace filelink {
namespace db {

void ObjectDao::create(const Object& object) {
    sql_ << "INSERT INTO objects (content_hash, byte_size, ref_count, state) "
            "VALUES (:hash, :size, :ref_count, :state)",
            soci::use(object.content_hash),
            soci::use(object.byte_size),
            soci::use(object.ref_count),
            soci::use(object.state);
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
