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

class LocalObjectStore {
public:
    explicit LocalObjectStore(std::string storageRoot);

    CommitResult commit(
        const std::string& tempPath,
        const std::string& contentHash) const;

private:
    std::string storageRoot_;
};

} // namespace filelink
