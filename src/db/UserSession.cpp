#include "UserSession.h"

namespace filelink {
namespace db {

void UserSessionDao::create(const UserSession& session) {
    sql_ << "INSERT INTO user_sessions (token_hash, user_id, expires_at) "
            "VALUES (:token_hash, :user_id, :expires_at)",
            soci::use(session.token_hash),
            soci::use(session.user_id),
            soci::use(session.expires_at);
}

bool UserSessionDao::find_active(const std::string& token_hash, UserSession& out_session) {
    soci::indicator result_ind;

    sql_ << "SELECT token_hash, user_id, expires_at "
            "FROM user_sessions "
            "WHERE token_hash = :token_hash AND expires_at > NOW()",
            soci::into(out_session.token_hash, result_ind),
            soci::into(out_session.user_id),
            soci::into(out_session.expires_at),
            soci::use(token_hash);

    return result_ind == soci::i_ok;
}

void UserSessionDao::remove(const std::string& token_hash) {
    sql_ << "DELETE FROM user_sessions WHERE token_hash = :token_hash",
            soci::use(token_hash);
}

} // namespace db
} // namespace filelink
