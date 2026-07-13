#pragma once

#include <soci/soci.h>

#include <string>

namespace filelink {
namespace db {

struct User {
    std::string user_id;       // BINARY(16) mapped to std::string
    std::string username;
    std::string password_hash;
    bool is_disabled = false;
};

class UserDao {
public:
    explicit UserDao(soci::session& sql) : sql_(sql) {}

    void create(const User& user);
    bool find_by_id(const std::string& user_id, User& out_user);
    bool find_by_username(const std::string& username, User& out_user);

private:
    soci::session& sql_;
};

} // namespace db
} // namespace filelink
