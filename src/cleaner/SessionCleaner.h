#pragma once

#include <string>

namespace soci {
class connection_pool;
}

namespace filelink {

class SessionCleaner {
public:
    SessionCleaner(soci::connection_pool& pool, std::string storageRoot);
    ~SessionCleaner() = default;

    SessionCleaner(const SessionCleaner&) = delete;
    SessionCleaner& operator=(const SessionCleaner&) = delete;

    /**
     * @brief 扫描并物理清理已过期的上传会话及临时文件
     * @return 成功清理的会话数量
     */
    int cleanup_expired_sessions();

private:
    soci::connection_pool& pool_;
    std::string storageRoot_;
};

} // namespace filelink
