#include "AuthService.h"

#include "PasswordHasher.h"
#include "db/User.h"
#include "db/UserSession.h"

#include <soci/connection-pool.h>
#include <soci/soci.h>
#include <sodium.h>

#include <array>
#include <cctype>
#include <ctime>
#include <stdexcept>

namespace filelink {

namespace {

class SociSessionLease {
public:
    explicit SociSessionLease(soci::connection_pool& pool) : pool_(pool), pos_(pool.lease()) {}
    ~SociSessionLease() { pool_.give_back(pos_); }

    soci::session& get() { return pool_.at(pos_); }

private:
    soci::connection_pool& pool_;
    std::size_t pos_;
};

bool is_valid_username(const std::string& username) {
    if (username.size() < 3 || username.size() > 64) {
        return false;
    }

    for (unsigned char value : username) {
        if (!std::isalnum(value) && value != '_' && value != '-') {
            return false;
        }
    }
    return true;
}

std::string random_bytes(std::size_t size) {
    std::string value(size, '\0');
    randombytes_buf(&value[0], value.size());
    return value;
}

std::string hash_token(const std::string& token) {
    std::array<unsigned char, crypto_generichash_BYTES> hash{};
    crypto_generichash(hash.data(), hash.size(),
        reinterpret_cast<const unsigned char*>(token.data()), token.size(), nullptr, 0);
    return std::string(reinterpret_cast<const char*>(hash.data()), hash.size());
}

std::string encode_token(const std::string& token) {
    std::array<char, crypto_generichash_BYTES * 2 + 1> encoded{};
    sodium_bin2hex(encoded.data(), encoded.size(),
        reinterpret_cast<const unsigned char*>(token.data()), token.size());
    return encoded.data();
}

std::tm session_expiry() {
    const std::time_t value = std::time(nullptr) + 7 * 24 * 60 * 60;
    std::tm result{};
    localtime_r(&value, &result);
    return result;
}

} // namespace

RegisterResult AuthService::register_user(const std::string& username,
    const std::string& password,
    Registration& out_registration) {
    if (!is_valid_username(username)) {
        return RegisterResult::InvalidUsername;
    }
    if (password.size() < 8 || password.size() > 128) {
        return RegisterResult::InvalidPassword;
    }
    if (sodium_init() < 0) {
        return RegisterResult::SystemError;
    }

    try {
        SociSessionLease lease(pool_);
        soci::session& sql = lease.get();
        db::UserDao users(sql);
        db::User existing;
        if (users.find_by_username(username, existing)) {
            return RegisterResult::UsernameTaken;
        }

        const std::string user_id = random_bytes(16);
        const std::string token = random_bytes(crypto_generichash_BYTES);
        db::User user;
        user.user_id = user_id;
        user.username = username;
        user.password_hash = PasswordHasher::hash(password);

        db::UserSession session;
        session.token_hash = hash_token(token);
        session.user_id = user_id;
        session.expires_at = session_expiry();

        soci::transaction transaction(sql);
        users.create(user);
        db::UserSessionDao(sql).create(session);
        transaction.commit();

        out_registration = Registration{user_id, username, encode_token(token)};
        return RegisterResult::Success;
    } catch (const std::exception&) {
        return RegisterResult::SystemError;
    }
}

} // namespace filelink
