#include "UploadSessionStore.h"

namespace filelink {
namespace store {

void UploadSessionStore::create(const models::UploadSession& session) {
    soci::indicator expected_hash_ind = session.has_expected_hash ? soci::i_ok : soci::i_null;
    soci::indicator content_hash_ind = session.has_content_hash ? soci::i_ok : soci::i_null;
    soci::indicator failure_reason_ind = session.has_failure_reason ? soci::i_ok : soci::i_null;

    sql_ << "INSERT INTO upload_sessions ("
            "upload_id, state, file_name, total_size, committed_offset, "
            "expected_hash, content_hash, failure_reason, expires_at) "
            "VALUES (:id, :state, :name, :size, :offset, :eh, :ch, :fr, :expires)",
            soci::use(session.upload_id),
            soci::use(session.state),
            soci::use(session.file_name),
            soci::use(session.total_size),
            soci::use(session.committed_offset),
            soci::use(session.expected_hash, expected_hash_ind),
            soci::use(session.content_hash, content_hash_ind),
            soci::use(session.failure_reason, failure_reason_ind),
            soci::use(session.expires_at);
}

bool UploadSessionStore::find(const std::string& upload_id, models::UploadSession& session) {
    soci::indicator expected_hash_ind;
    soci::indicator content_hash_ind;
    soci::indicator failure_reason_ind;
    soci::indicator result_ind;

    sql_ << "SELECT upload_id, state, file_name, total_size, committed_offset, "
            "expected_hash, content_hash, failure_reason, created_at, updated_at, expires_at "
            "FROM upload_sessions WHERE upload_id = :id",
            soci::into(session.upload_id, result_ind),
            soci::into(session.state),
            soci::into(session.file_name),
            soci::into(session.total_size),
            soci::into(session.committed_offset),
            soci::into(session.expected_hash, expected_hash_ind),
            soci::into(session.content_hash, content_hash_ind),
            soci::into(session.failure_reason, failure_reason_ind),
            soci::into(session.created_at),
            soci::into(session.updated_at),
            soci::into(session.expires_at),
            soci::use(upload_id);

    if (result_ind != soci::i_ok) {
        return false;
    }

    session.has_expected_hash = (expected_hash_ind == soci::i_ok);
    session.has_content_hash = (content_hash_ind == soci::i_ok);
    session.has_failure_reason = (failure_reason_ind == soci::i_ok);

    return true;
}

} // namespace store
} // namespace filelink
