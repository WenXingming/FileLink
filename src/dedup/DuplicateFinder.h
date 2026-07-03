#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "FileInfo.h"
#include "FileWalker.h"
#include "Hasher.h"

namespace wxm {
class ThreadPool;
}

struct DuplicateGroup {
    uint64_t size;
    Blake3Value hash;
    std::vector<std::string> paths;
};

struct DuplicateReport {
    size_t scannedFiles = 0;
    size_t sizeSkippedFiles = 0;
    size_t partialHashedFiles = 0;
    size_t fullHashedFiles = 0;
    uint64_t partialHashBytesRead = 0;
    uint64_t fullHashBytesRead = 0;
    size_t hashTasks = 0;
    std::vector<DuplicateGroup> duplicateGroups;
    std::vector<FileError> errors;
};

class DuplicateFinder {
public:
    explicit DuplicateFinder(wxm::ThreadPool& pool, const FileWalker& fileWalker, const Hasher& hasher);

    DuplicateReport find_duplicates(const std::string& rootPath) const;

private:
    std::vector<FileInfo> find_size_collisions(const std::vector<FileInfo>& files) const;
    void split_candidates_by_threshold(const std::vector<FileInfo>& candidates, uint64_t threshold, std::vector<FileInfo>& smallOut, std::vector<FileInfo>& largeOut) const;
    std::vector<std::vector<FileInfo> > create_batches(const std::vector<FileInfo>& candidates, size_t batchSize) const;
    std::vector<HashResult> compute_partial_hashes(const std::vector<std::vector<FileInfo> >& batches) const;
    std::vector<FileInfo> find_partial_collisions(const std::vector<HashResult>& partialHashes) const;
    std::vector<HashResult> compute_full_hashes(const std::vector<std::vector<FileInfo> >& batches) const;
    std::vector<DuplicateGroup> find_exact_duplicates(const std::vector<HashResult>& fullHashes) const;
    DuplicateReport assemble_report(const FileWalkResult& walkResult, const std::vector<HashResult>& partialHashes, const std::vector<HashResult>& fullHashes, const std::vector<DuplicateGroup>& duplicateGroups) const;

    wxm::ThreadPool& pool_;
    const FileWalker& fileWalker_;
    const Hasher& hasher_;
};
