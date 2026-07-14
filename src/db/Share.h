#pragma once

#include <soci/soci.h>

#include <ctime>
#include <string>
#include <vector>

namespace filelink {
namespace db {

// ====================================================================
// Share：一个逻辑文件的可撤销、带过期时间的公开访问授权。
// ====================================================================
struct Share {
    std::string share_id;   // BINARY(16) mapped to std::string
    std::string file_id;    // BINARY(16) mapped to std::string
    std::string token_hash; // BINARY(32) mapped to std::string
    std::tm expires_at{};
    std::tm created_at{};
};

// =====================================================================
// ShareDao：创建、查找并撤销逻辑文件的有效分享授权。
// =====================================================================
class ShareDao {
public:
    explicit ShareDao(soci::session& sql) : sql_(sql) {}

    void create(const Share& share);
    void find_active_by_file_id(const std::string& file_id, std::vector<Share>& out_shares);
    bool find_active_by_token_hash(const std::string& token_hash, Share& out_share);
    bool revoke_by_id_and_file_id(const std::string& share_id, const std::string& file_id);

private:
    soci::session& sql_;
};

} // namespace db
} // namespace filelink
