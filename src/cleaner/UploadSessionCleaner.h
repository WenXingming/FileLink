#pragma once

#include <string>

namespace soci {
class connection_pool;
}

namespace filelink {

// ===========================================================================
// UploadSessionCleaner：清理已终止（取消上传或过期）上传会话及其尚未发布的临时分片文件。
// ===========================================================================
class UploadSessionCleaner {
public:
    UploadSessionCleaner(soci::connection_pool& pool, std::string storageRoot);
    ~UploadSessionCleaner() = default;

    UploadSessionCleaner(const UploadSessionCleaner&) = delete;
    UploadSessionCleaner& operator=(const UploadSessionCleaner&) = delete;

    int cleanup_terminated_sessions();

private:
    soci::connection_pool& pool_;
    std::string storageRoot_;
};

} // namespace filelink
