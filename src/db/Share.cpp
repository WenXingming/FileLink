#include "Share.h"

#include <soci/rowset.h>

namespace filelink {
namespace db {

void ShareDao::create(const Share& share) {
    sql_ << "INSERT INTO shares (share_id, file_id, token_hash, expires_at) "
            "VALUES (:share_id, :file_id, :token_hash, :expires_at)",
            soci::use(share.share_id),
            soci::use(share.file_id),
            soci::use(share.token_hash),
            soci::use(share.expires_at);
}

void ShareDao::find_active_by_file_id(const std::string& file_id,
    std::vector<Share>& out_shares) {
    soci::rowset<soci::row> rows = (sql_.prepare
        << "SELECT share_id, file_id, token_hash, expires_at, created_at "
           "FROM shares WHERE file_id = :file_id "
           "AND revoked_at IS NULL AND expires_at > NOW(6) "
           "ORDER BY created_at DESC, share_id DESC",
        soci::use(file_id));

    out_shares.clear();
    for (const soci::row& row : rows) {
        out_shares.push_back(Share{
            row.get<std::string>(0),
            row.get<std::string>(1),
            row.get<std::string>(2),
            row.get<std::tm>(3),
            row.get<std::tm>(4)
        });
    }
}

bool ShareDao::find_active_by_token_hash(const std::string& token_hash, Share& out_share) {
    soci::indicator result_ind;

    sql_ << "SELECT share_id, file_id, token_hash, expires_at, created_at "
            "FROM shares WHERE token_hash = :token_hash "
            "AND revoked_at IS NULL AND expires_at > NOW(6)",
            soci::into(out_share.share_id, result_ind),
            soci::into(out_share.file_id),
            soci::into(out_share.token_hash),
            soci::into(out_share.expires_at),
            soci::into(out_share.created_at),
            soci::use(token_hash);

    return result_ind == soci::i_ok;
}

bool ShareDao::revoke_by_id_and_file_id(const std::string& share_id, const std::string& file_id) {
    soci::statement statement = (sql_.prepare
        << "UPDATE shares SET revoked_at = NOW(6) "
           "WHERE share_id = :share_id AND file_id = :file_id AND revoked_at IS NULL",
        soci::use(share_id),
        soci::use(file_id));
    statement.execute(false);
    return statement.get_affected_rows() == 1;
}

} // namespace db
} // namespace filelink
