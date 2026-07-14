#include "AuthService.h"

#include "PasswordHasher.h"
#include "redis/UserSessionCache.h"
#include "database/SociSessionLease.h"
#include "database/User.h"
#include "database/UserSession.h"

#include <soci/connection-pool.h>
#include <soci/soci.h>
#include <sodium.h>

#include <array>
#include <cctype>
#include <ctime>
#include <stdexcept>

namespace filelink {

namespace {

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

bool decode_token(const std::string& encoded, std::string& token) {
    if (encoded.size() != crypto_generichash_BYTES * 2) {
        return false;
    }

    std::array<unsigned char, crypto_generichash_BYTES> decoded{};
    std::size_t decoded_size = 0;
    if (sodium_hex2bin(decoded.data(), decoded.size(), encoded.data(), encoded.size(),
            nullptr, &decoded_size, nullptr)
        != 0
        || decoded_size != decoded.size()) {
        return false;
    }

    token.assign(reinterpret_cast<const char*>(decoded.data()), decoded.size());
    return true;
}

std::tm session_expiry() {
    const std::time_t value = std::time(nullptr) + 7 * 24 * 60 * 60;
    std::tm result{};
    localtime_r(&value, &result);
    return result;
}

unsigned int remaining_session_seconds(const std::tm& expires_at) {
    std::tm local_expiry = expires_at;
    const std::time_t expiry = std::mktime(&local_expiry);
    const std::time_t now = std::time(nullptr);
    if (expiry <= now) {
        return 0;
    }
    return static_cast<unsigned int>(expiry - now);
}

AuthenticatedSession create_session(soci::session& sql, const db::User& user) {
    const std::string token = random_bytes(crypto_generichash_BYTES);

    db::UserSession session;
    session.token_hash = hash_token(token);
    session.user_id = user.user_id;
    session.expires_at = session_expiry();
    db::UserSessionDao(sql).create(session);

    return AuthenticatedSession{user.user_id, user.username, encode_token(token)};
}

} // namespace

RegisterResult AuthService::register_user(const std::string& username,
    const std::string& password,
    AuthenticatedSession& out_session) {
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
        db::SociSessionLease lease(pool_);
        soci::session& sql = lease.get();
        db::UserDao users(sql);
        db::User existing;
        if (users.find_by_username(username, existing)) {
            return RegisterResult::UsernameTaken;
        }

        const std::string user_id = random_bytes(16);
        db::User user;
        user.user_id = user_id;
        user.username = username;
        user.password_hash = PasswordHasher::hash(password);

        soci::transaction transaction(sql);
        users.create(user);
        AuthenticatedSession session = create_session(sql, user);
        transaction.commit();

        out_session = session;
        return RegisterResult::Success;
    } catch (const std::exception&) {
        return RegisterResult::SystemError;
    }
}

LoginResult AuthService::login_user(const std::string& username,
    const std::string& password,
    AuthenticatedSession& out_session) {
    try {
        db::SociSessionLease lease(pool_);
        soci::session& sql = lease.get();
        db::User user;
        if (!db::UserDao(sql).find_by_username(username, user)
            || user.is_disabled
            || !PasswordHasher::verify(password, user.password_hash)) {
            return LoginResult::InvalidCredentials;
        }

        soci::transaction transaction(sql);
        AuthenticatedSession session = create_session(sql, user);
        transaction.commit();

        out_session = session;
        return LoginResult::Success;
    } catch (const std::exception&) {
        return LoginResult::SystemError;
    }
}

CurrentUserResult AuthService::current_user(const std::string& session_token,
    AuthenticatedUser& out_user) {
    if (sodium_init() < 0) {
        return CurrentUserResult::SystemError;
    }

    std::string token;
    if (!decode_token(session_token, token)) {
        return CurrentUserResult::InvalidSession;
    }

    const std::string token_hash = hash_token(token);
    std::string user_id;
    if (session_cache_ != nullptr
        && session_cache_->find_user_id(token_hash, user_id) == redis::CacheLookupResult::Hit) {
        try {
            db::SociSessionLease lease(pool_);
            db::User user;
            if (!db::UserDao(lease.get()).find_by_id(user_id, user) || user.is_disabled) {
                session_cache_->remove(token_hash);
                return CurrentUserResult::InvalidSession;
            }
            out_user = {user.user_id, user.username};
            return CurrentUserResult::Success;
        } catch (const std::exception&) {
            return CurrentUserResult::SystemError;
        }
    }

    try {
        db::SociSessionLease lease(pool_);
        soci::session& sql = lease.get();
        db::UserSession session;
        if (!db::UserSessionDao(sql).find_active(token_hash, session)) {
            return CurrentUserResult::InvalidSession;
        }

        db::User user;
        if (!db::UserDao(sql).find_by_id(session.user_id, user) || user.is_disabled) {
            return CurrentUserResult::InvalidSession;
        }

        out_user = {user.user_id, user.username};
        if (session_cache_ != nullptr) {
            session_cache_->store_user_id(token_hash, user.user_id,
                remaining_session_seconds(session.expires_at));
        }
        return CurrentUserResult::Success;
    } catch (const std::exception&) {
        return CurrentUserResult::SystemError;
    }
}

LogoutResult AuthService::logout(const std::string& session_token) {
    if (sodium_init() < 0) {
        return LogoutResult::SystemError;
    }

    std::string token;
    if (!decode_token(session_token, token)) {
        return LogoutResult::InvalidSession;
    }

    const std::string token_hash = hash_token(token);
    try {
        db::SociSessionLease lease(pool_);
        db::UserSessionDao(lease.get()).remove(token_hash);
        if (session_cache_ != nullptr) {
            session_cache_->remove(token_hash);
        }
        return LogoutResult::Success;
    } catch (const std::exception&) {
        return LogoutResult::SystemError;
    }
}

} // namespace filelink
