#include "cache/RedisSessionCache.h"

#include <gtest/gtest.h>

#include <cstdlib>

namespace filelink {
namespace {

bool redis_test_config(cache::RedisConfig& out_config) {
    const char* port = std::getenv("FILELINK_TEST_REDIS_PORT");
    if (port == nullptr || port[0] == '\0') {
        return false;
    }

    out_config.enabled = true;
    out_config.port = static_cast<uint16_t>(std::strtoul(port, nullptr, 10));
    return out_config.port != 0;
}

} // namespace

TEST(RedisSessionCacheTest, StoresReadsAndRemovesHashedSessionMappings) {
    cache::RedisConfig config;
    if (!redis_test_config(config)) {
        GTEST_SKIP() << "set FILELINK_TEST_REDIS_PORT to run Redis integration coverage";
    }

    cache::RedisSessionCache session_cache(config);
    const std::string token_hash(32, 't');
    const std::string user_id(16, 'u');
    std::string cached_user_id;

    session_cache.remove(token_hash);
    EXPECT_EQ(session_cache.find_user_id(token_hash, cached_user_id), cache::CacheLookupResult::Miss);

    session_cache.store_user_id(token_hash, user_id, 60);
    EXPECT_EQ(session_cache.find_user_id(token_hash, cached_user_id), cache::CacheLookupResult::Hit);
    EXPECT_EQ(cached_user_id, user_id);

    session_cache.remove(token_hash);
    EXPECT_EQ(session_cache.find_user_id(token_hash, cached_user_id), cache::CacheLookupResult::Miss);
}

} // namespace filelink
