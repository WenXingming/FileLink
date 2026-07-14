#include "User.h"

#include <ctime>

namespace filelink {
namespace db {

void UserDao::create(const User& user) {
    sql_ << "INSERT INTO users (user_id, username, password_hash) "
            "VALUES (:id, :username, :password_hash)",
            soci::use(user.user_id),
            soci::use(user.username),
            soci::use(user.password_hash);
}

bool UserDao::find_by_id(const std::string& user_id, User& out_user) {
    soci::indicator result_ind;
    soci::indicator disabled_ind;
    std::tm disabled_at{};

    sql_ << "SELECT user_id, username, password_hash, disabled_at "
            "FROM users WHERE user_id = :id",
            soci::into(out_user.user_id, result_ind),
            soci::into(out_user.username),
            soci::into(out_user.password_hash),
            soci::into(disabled_at, disabled_ind),
            soci::use(user_id);

    if (result_ind != soci::i_ok) {
        return false;
    }

    out_user.is_disabled = (disabled_ind == soci::i_ok);
    return true;
}

bool UserDao::find_by_username(const std::string& username, User& out_user) {
    soci::indicator result_ind;
    soci::indicator disabled_ind;
    std::tm disabled_at{};

    sql_ << "SELECT user_id, username, password_hash, disabled_at "
            "FROM users WHERE username = :username",
            soci::into(out_user.user_id, result_ind),
            soci::into(out_user.username),
            soci::into(out_user.password_hash),
            soci::into(disabled_at, disabled_ind),
            soci::use(username);

    if (result_ind != soci::i_ok) {
        return false;
    }

    out_user.is_disabled = (disabled_ind == soci::i_ok);
    return true;
}

} // namespace db
} // namespace filelink
