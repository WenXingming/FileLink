#include "UploadSession.h"

namespace filelink {
namespace db {

void UploadSessionDao::create(const UploadSession& session) {
    soci::indicator expected_hash_ind = session.has_expected_hash ? soci::i_ok : soci::i_null;
    soci::indicator content_hash_ind = session.has_content_hash ? soci::i_ok : soci::i_null;
    soci::indicator failure_reason_ind = session.has_failure_reason ? soci::i_ok : soci::i_null;

    sql_ << "INSERT INTO upload_sessions ("
            "upload_id, owner_user_id, state, file_name, total_size, committed_offset, "
            "expected_hash, content_hash, failure_reason, expires_at) "
            "VALUES (:id, :owner, :state, :name, :size, :offset, :eh, :ch, :fr, :expires)",
            soci::use(session.upload_id),
            soci::use(session.owner_user_id),
            soci::use(session.state),
            soci::use(session.file_name),
            soci::use(session.total_size),
            soci::use(session.committed_offset),
            soci::use(session.expected_hash, expected_hash_ind),
            soci::use(session.content_hash, content_hash_ind),
            soci::use(session.failure_reason, failure_reason_ind),
            soci::use(session.expires_at);
}

bool UploadSessionDao::find(const std::string& upload_id, UploadSession& session) {
    soci::indicator expected_hash_ind;
    soci::indicator content_hash_ind;
    soci::indicator failure_reason_ind;
    soci::indicator result_ind;

    sql_ << "SELECT upload_id, owner_user_id, state, file_name, total_size, committed_offset, "
            "expected_hash, content_hash, failure_reason, created_at, updated_at, expires_at "
            "FROM upload_sessions WHERE upload_id = :id",
            soci::into(session.upload_id, result_ind),
            soci::into(session.owner_user_id),
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

void UploadSessionDao::update_offset(const std::string& upload_id, uint64_t new_offset) {
    sql_ << "UPDATE upload_sessions SET committed_offset = :offset WHERE upload_id = :id",
            soci::use(new_offset),
            soci::use(upload_id);
}

void UploadSessionDao::update_state(const std::string& upload_id, const std::string& state) {
    sql_ << "UPDATE upload_sessions SET state = :state WHERE upload_id = :id",
            soci::use(state),
            soci::use(upload_id);
}

void UploadSessionDao::update_completed(const std::string& upload_id, const std::string& content_hash) {
    sql_ << "UPDATE upload_sessions SET state = 'COMPLETED', content_hash = :hash WHERE upload_id = :id",
            soci::use(content_hash),
            soci::use(upload_id);
}

void UploadSessionDao::update_failed(const std::string& upload_id, const std::string& failure_reason) {
    sql_ << "UPDATE upload_sessions SET state = 'FAILED', failure_reason = :reason WHERE upload_id = :id",
            soci::use(failure_reason),
            soci::use(upload_id);
}

} // namespace db
} // namespace filelink
