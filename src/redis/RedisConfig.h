#pragma once

#include <cstdint>
#include <string>

namespace filelink {
namespace redis {

// =============================================================
// RedisConfig：连接 Redis 会话缓存所需的运行时配置。
// =============================================================
struct RedisConfig {
    bool enabled = false;
    std::string host = "127.0.0.1";
    uint16_t port = 6379;
    unsigned int timeoutMilliseconds = 100;
};

} // namespace redis
} // namespace filelink
