#pragma once

#include <soci/soci.h>

#include <ctime>
#include <string>

namespace filelink {
namespace db {

// ==================================================================
// UserSession：保存会话令牌哈希、归属用户和失效时间的记录。
// ==================================================================
struct UserSession {
    std::string token_hash;    // BINARY(32) mapped to std::string
    std::string user_id;       // BINARY(16) mapped to std::string
    std::tm expires_at{};
};

// =================================================================
// UserSessionDao：创建、查询和删除服务端会话持久化记录。
// =================================================================
class UserSessionDao {
public:
    explicit UserSessionDao(soci::session& sql) : sql_(sql) {}

    void create(const UserSession& session);
    bool find_active(const std::string& token_hash, UserSession& out_session);
    void remove(const std::string& token_hash);

private:
    soci::session& sql_;
};

} // namespace db
} // namespace filelink
