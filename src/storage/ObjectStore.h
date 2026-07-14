#pragma once

#include <string>

namespace filelink {

// =======================================================
// CommitStatus：对象发布到内容存储后的最终状态。
// =======================================================
enum class CommitStatus {
    Created,
    Reused
};

// ===========================================================
// CommitResult：对象发布结果及可读取的正式存储路径。
// ===========================================================
struct CommitResult {
    CommitStatus status;
    std::string objectPath;
};

// =====================================================================
// ObjectStore：按内容哈希原子发布和定位不可变物理对象。
// =====================================================================
class ObjectStore {
public:
    explicit ObjectStore(std::string storageRoot);

    CommitResult commit(const std::string& tempPath, const std::string& contentHash) const;

    std::string get_object_path(const std::string& contentHash) const;

private:
    std::string storageRoot_;
};

} // namespace filelink
