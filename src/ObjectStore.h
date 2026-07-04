#pragma once

#include <string>

namespace filelink {

enum class CommitStatus {
    Created,
    Reused
};

struct CommitResult {
    CommitStatus status;
    std::string objectPath;
};

class ObjectStore {
public:
    explicit ObjectStore(std::string storageRoot);

    CommitResult commit(const std::string& tempPath, const std::string& contentHash) const;

    std::string get_object_path(const std::string& contentHash) const;

private:
    std::string storageRoot_;
};

} // namespace filelink
