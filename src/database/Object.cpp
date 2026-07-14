#include "Object.h"

#include <soci/rowset.h>

namespace filelink {
namespace db {

ObjectReferenceResult ObjectDao::add_reference(const std::string& content_hash, uint64_t byte_size) {
    sql_ << "INSERT INTO objects (content_hash, byte_size, ref_count, state) "
            "VALUES (:hash, :size, 1, 'READY') "
            "ON DUPLICATE KEY UPDATE "
            "ref_count = CASE WHEN state = 'RECLAIMING' THEN ref_count ELSE ref_count + 1 END, "
            "state = CASE WHEN state = 'RECLAIMING' THEN state ELSE 'READY' END",
            soci::use(content_hash),
            soci::use(byte_size);

    Object object;
    if (!find(content_hash, object) || object.state == "RECLAIMING") {
        return ObjectReferenceResult::Reclaiming;
    }
    return ObjectReferenceResult::Referenced;
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

bool ObjectDao::claim_pending_delete(const std::string& content_hash) {
    soci::statement statement = (sql_.prepare
        << "UPDATE objects SET state = 'RECLAIMING' "
           "WHERE content_hash = :hash AND ref_count = 0 AND state = 'PENDING_DELETE'",
        soci::use(content_hash));
    statement.execute(false);
    return statement.get_affected_rows() == 1;
}

bool ObjectDao::return_to_pending_delete(const std::string& content_hash) {
    soci::statement statement = (sql_.prepare
        << "UPDATE objects SET state = 'PENDING_DELETE' "
           "WHERE content_hash = :hash AND ref_count = 0 AND state = 'RECLAIMING'",
        soci::use(content_hash));
    statement.execute(false);
    return statement.get_affected_rows() == 1;
}

void ObjectDao::find_reclaimable(std::vector<Object>& out_objects) {
    soci::rowset<soci::row> rows = (sql_.prepare
        << "SELECT content_hash, state FROM objects "
           "WHERE ref_count = 0 AND state IN ('PENDING_DELETE', 'RECLAIMING')");

    out_objects.clear();
    for (const soci::row& row : rows) {
        out_objects.push_back(Object{
            row.get<std::string>(0),
            0,
            0,
            row.get<std::string>(1)
        });
    }
}

bool ObjectDao::remove_reclaiming(const std::string& content_hash) {
    soci::statement statement = (sql_.prepare
        << "DELETE FROM objects "
           "WHERE content_hash = :hash AND ref_count = 0 AND state = 'RECLAIMING'",
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
