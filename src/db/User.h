#pragma once

#include <soci/soci.h>

#include <string>

namespace filelink {
namespace db {

// =========================================================
// User：账户身份、密码哈希和禁用状态的持久化模型。
// =========================================================
struct User {
    std::string user_id;       // BINARY(16) mapped to std::string
    std::string username;
    std::string password_hash;
    bool is_disabled = false;
};

// ==========================================================
// UserDao：创建用户，并按 ID 或用户名查询用户记录。
// ==========================================================
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
