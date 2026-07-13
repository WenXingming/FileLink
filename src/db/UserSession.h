#pragma once

#include <soci/soci.h>

#include <ctime>
#include <string>

namespace filelink {
namespace db {

struct UserSession {
    std::string token_hash;    // BINARY(32) mapped to std::string
    std::string user_id;       // BINARY(16) mapped to std::string
    std::tm expires_at{};
};

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
