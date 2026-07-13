#pragma once

#include <string>

namespace soci {
class connection_pool;
}

namespace filelink {

// =====================================================================
// SessionCleaner：清理过期上传会话及其尚未发布的临时分片文件。
// =====================================================================
class SessionCleaner {
public:
    SessionCleaner(soci::connection_pool& pool, std::string storageRoot);
    ~SessionCleaner() = default;

    SessionCleaner(const SessionCleaner&) = delete;
    SessionCleaner& operator=(const SessionCleaner&) = delete;

    int cleanup_expired_sessions();

private:
    soci::connection_pool& pool_;
    std::string storageRoot_;
};

} // namespace filelink
