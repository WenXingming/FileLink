#include "ShareService.h"

#include "database/File.h"
#include "database/SociSessionLease.h"

#include <soci/soci.h>
#include <sodium.h>

#include <array>
#include <stdexcept>

namespace filelink {

namespace {

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

bool decode_token(const std::string& encoded, std::string& out_token) {
    if (encoded.size() != crypto_generichash_BYTES * 2) {
        return false;
    }

    std::string token(crypto_generichash_BYTES, '\0');
    if (sodium_hex2bin(reinterpret_cast<unsigned char*>(&token[0]), token.size(),
            encoded.data(), encoded.size(), nullptr, nullptr, nullptr) != 0) {
        return false;
    }
    out_token = token;
    return true;
}

std::tm local_time(std::time_t value) {
    std::tm result{};
    localtime_r(&value, &result);
    return result;
}

} // namespace

CreateShareResult ShareService::create_share(const std::string& owner_user_id,
    const std::string& file_id,
    std::time_t expires_at,
    CreatedShare& out_share) {
    if (expires_at <= std::time(nullptr)) {
        return CreateShareResult::InvalidExpiry;
    }
    if (sodium_init() < 0) {
        return CreateShareResult::SystemError;
    }

    try {
        db::SociSessionLease lease(pool_);
        soci::session& sql = lease.get();
        db::File file;
        if (!db::FileDao(sql).find_by_id_and_owner(file_id, owner_user_id, file)) {
            return CreateShareResult::FileNotFound;
        }

        const std::string token = random_bytes(crypto_generichash_BYTES);
        db::Share share;
        share.share_id = random_bytes(16);
        share.file_id = file_id;
        share.token_hash = hash_token(token);
        share.expires_at = local_time(expires_at);

        soci::transaction transaction(sql);
        db::ShareDao(sql).create(share);
        transaction.commit();

        out_share = { share.share_id, encode_token(token), share.expires_at };
        return CreateShareResult::Success;
    } catch (const std::exception&) {
        return CreateShareResult::SystemError;
    }
}

bool ShareService::list_shares(const std::string& owner_user_id,
    const std::string& file_id,
    std::vector<db::Share>& out_shares) {
    db::SociSessionLease lease(pool_);
    soci::session& sql = lease.get();
    db::File file;
    if (!db::FileDao(sql).find_by_id_and_owner(file_id, owner_user_id, file)) {
        return false;
    }

    db::ShareDao(sql).find_active_by_file_id(file_id, out_shares);
    return true;
}

bool ShareService::find_shared_file(const std::string& token, db::File& out_file) {
    if (sodium_init() < 0) {
        return false;
    }

    std::string raw_token;
    if (!decode_token(token, raw_token)) {
        return false;
    }

    try {
        db::SociSessionLease lease(pool_);
        soci::session& sql = lease.get();
        db::Share share;
        if (!db::ShareDao(sql).find_active_by_token_hash(hash_token(raw_token), share)) {
            return false;
        }
        return db::FileDao(sql).find_by_id(share.file_id, out_file);
    } catch (const std::exception&) {
        return false;
    }
}

RevokeShareResult ShareService::revoke_share(const std::string& owner_user_id,
    const std::string& file_id,
    const std::string& share_id) {
    try {
        db::SociSessionLease lease(pool_);
        soci::session& sql = lease.get();
        db::File file;
        if (!db::FileDao(sql).find_by_id_and_owner(file_id, owner_user_id, file)) {
            return RevokeShareResult::FileNotFound;
        }
        if (!db::ShareDao(sql).revoke_by_id_and_file_id(share_id, file_id)) {
            return RevokeShareResult::ShareNotFound;
        }
        return RevokeShareResult::Success;
    } catch (const std::exception&) {
        return RevokeShareResult::SystemError;
    }
}

} // namespace filelink
