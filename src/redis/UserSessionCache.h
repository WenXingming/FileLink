#pragma once

#include "RedisConfig.h"

#include <mutex>
#include <string>
#include <utility>

struct redisContext;

namespace filelink {
namespace redis {

// ===================================================================
// CacheLookupResult：Redis 查询会话缓存后的命中和可用性状态。
// ===================================================================
enum class CacheLookupResult {
    Hit,
    Miss,
    Unavailable
};

// ========================================================================
// UserSessionCache：缓存 token_hash 到 user_id 的短生命周期会话映射。
// ========================================================================
class UserSessionCache {
public:
    explicit UserSessionCache(RedisConfig config) : config_(std::move(config)) {}
    ~UserSessionCache();

    UserSessionCache(const UserSessionCache&) = delete;
    UserSessionCache& operator=(const UserSessionCache&) = delete;

    CacheLookupResult find_user_id(const std::string& token_hash, std::string& out_user_id);
    void store_user_id(const std::string& token_hash, const std::string& user_id, unsigned int ttl_seconds);
    void remove(const std::string& token_hash);

private:
    redisContext* connection_locked();
    void reset_connection_locked();

private:
    RedisConfig config_;
    redisContext* context_ = nullptr;
    std::mutex mutex_;
};

} // namespace redis
} // namespace filelink
