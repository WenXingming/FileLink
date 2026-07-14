#include "UserSessionCache.h"

#include <hiredis/hiredis.h>

#include <memory>
#include <utility>

namespace filelink {
namespace redis {

namespace {

std::string hex_encode(const std::string& bytes) {
    const char digits[] = "0123456789abcdef";
    std::string encoded;
    encoded.reserve(bytes.size() * 2);
    for (unsigned char value : bytes) {
        encoded.push_back(digits[value >> 4]);
        encoded.push_back(digits[value & 0x0f]);
    }
    return encoded;
}

bool hex_decode(const std::string& encoded, std::string& out_bytes) {
    if (encoded.size() != 32) {
        return false;
    }

    const auto digit = [](char value) -> int {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        return -1;
    };

    out_bytes.clear();
    out_bytes.reserve(16);
    for (std::size_t index = 0; index < encoded.size(); index += 2) {
        const int high = digit(encoded[index]);
        const int low = digit(encoded[index + 1]);
        if (high < 0 || low < 0) {
            return false;
        }
        out_bytes.push_back(static_cast<char>((high << 4) | low));
    }
    return true;
}

std::string cache_key(const std::string& token_hash) {
    return "filelink:session:" + hex_encode(token_hash);
}

} // namespace

UserSessionCache::~UserSessionCache() {
    std::lock_guard<std::mutex> lock(mutex_);
    reset_connection_locked();
}

CacheLookupResult UserSessionCache::find_user_id(const std::string& token_hash,
    std::string& out_user_id) {
    if (!config_.enabled || token_hash.size() != 32) {
        return CacheLookupResult::Unavailable;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    redisContext* context = connection_locked();
    if (context == nullptr) {
        return CacheLookupResult::Unavailable;
    }

    const std::string key = cache_key(token_hash);
    std::unique_ptr<redisReply, decltype(&freeReplyObject)> reply(
        static_cast<redisReply*>(redisCommand(context, "GET %b", key.data(), key.size())),
        &freeReplyObject);
    if (!reply) {
        reset_connection_locked();
        return CacheLookupResult::Unavailable;
    }
    if (reply->type == REDIS_REPLY_NIL) {
        return CacheLookupResult::Miss;
    }
    if (reply->type != REDIS_REPLY_STRING
        || !hex_decode(std::string(reply->str, reply->len), out_user_id)) {
        return CacheLookupResult::Miss;
    }
    return CacheLookupResult::Hit;
}

void UserSessionCache::store_user_id(const std::string& token_hash,
    const std::string& user_id,
    unsigned int ttl_seconds) {
    if (!config_.enabled || token_hash.size() != 32 || user_id.size() != 16 || ttl_seconds == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    redisContext* context = connection_locked();
    if (context == nullptr) {
        return;
    }

    const std::string key = cache_key(token_hash);
    const std::string value = hex_encode(user_id);
    std::unique_ptr<redisReply, decltype(&freeReplyObject)> reply(
        static_cast<redisReply*>(redisCommand(context, "SET %b %b EX %u",
            key.data(), key.size(), value.data(), value.size(), ttl_seconds)),
        &freeReplyObject);
    if (!reply || reply->type == REDIS_REPLY_ERROR) {
        reset_connection_locked();
    }
}

void UserSessionCache::remove(const std::string& token_hash) {
    if (!config_.enabled || token_hash.size() != 32) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    redisContext* context = connection_locked();
    if (context == nullptr) {
        return;
    }

    const std::string key = cache_key(token_hash);
    std::unique_ptr<redisReply, decltype(&freeReplyObject)> reply(
        static_cast<redisReply*>(redisCommand(context, "DEL %b", key.data(), key.size())),
        &freeReplyObject);
    if (!reply || reply->type == REDIS_REPLY_ERROR) {
        reset_connection_locked();
    }
}

redisContext* UserSessionCache::connection_locked() {
    if (context_ != nullptr) {
        return context_;
    }

    const timeval timeout = {
        static_cast<long>(config_.timeoutMilliseconds / 1000),
        static_cast<long>((config_.timeoutMilliseconds % 1000) * 1000)
    };
    context_ = redisConnectWithTimeout(config_.host.c_str(), config_.port, timeout);
    if (context_ == nullptr || context_->err != 0 || redisSetTimeout(context_, timeout) != REDIS_OK) {
        reset_connection_locked();
        return nullptr;
    }
    return context_;
}

void UserSessionCache::reset_connection_locked() {
    if (context_ != nullptr) {
        redisFree(context_);
        context_ = nullptr;
    }
}

} // namespace redis
} // namespace filelink
