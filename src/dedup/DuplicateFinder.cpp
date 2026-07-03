#include "DuplicateFinder.h"
#include "ThreadPool.h"

#include <algorithm>
#include <future>

DuplicateFinder::DuplicateFinder(wxm::ThreadPool& pool, const FileWalker& fileWalker, const Hasher& hasher)
    : pool_(pool), fileWalker_(fileWalker), hasher_(hasher) {
}

DuplicateReport DuplicateFinder::find_duplicates(const std::string& rootPath) const {
    const FileWalkResult walkResult = fileWalker_.collect_files(rootPath);
    
    // ==========================================
    // 第一级漏斗：按文件大小碰撞
    // ==========================================
    const std::vector<FileInfo> sizeCollisions = find_size_collisions(walkResult.files);

    // ==========================================
    // 物理折叠：消除硬链接冗余 I/O
    // ==========================================
    std::vector<FileInfo> uniquePhysicalFiles;
    std::map<std::pair<uint64_t, uint64_t>, std::vector<FileInfo> > hardlinkFollowers;
    for (std::vector<FileInfo>::const_iterator it = sizeCollisions.begin(); it != sizeCollisions.end(); ++it) {
        std::pair<uint64_t, uint64_t> devIno = std::make_pair(it->device, it->inode);
        if (hardlinkFollowers.find(devIno) == hardlinkFollowers.end()) {
            uniquePhysicalFiles.push_back(*it);
            hardlinkFollowers[devIno] = std::vector<FileInfo>();
        } else {
            hardlinkFollowers[devIno].push_back(*it);
        }
    }

    // ==========================================
    // 智能分流道：保护小文件
    // ==========================================
    std::vector<FileInfo> smallCandidates;
    std::vector<FileInfo> largeCandidates;
    split_candidates_by_threshold(uniquePhysicalFiles, 4096, smallCandidates, largeCandidates);

    // ==========================================
    // 小文件物理排序：触发磁盘顺序读
    // ==========================================
    std::sort(smallCandidates.begin(), smallCandidates.end(), [](const FileInfo& left, const FileInfo& right) {
        if (left.device != right.device) return left.device < right.device;
        if (left.inode != right.inode) return left.inode < right.inode;
        return left.path < right.path;
    });

    // ==========================================
    // 第二级漏斗：大文件的局部哈希碰撞
    // ==========================================
    const size_t BATCH_SIZE = 64;
    const std::vector<std::vector<FileInfo> > partialBatches = create_batches(largeCandidates, BATCH_SIZE);
    const std::vector<HashResult> partialHashes = compute_partial_hashes(partialBatches);
    const std::vector<FileInfo> partialCollisions = find_partial_collisions(partialHashes);

    // ==========================================
    // 第三级漏斗：全量哈希碰撞（双轨并发调度）
    // ==========================================
    // 汇流：将过了第二级漏斗的大文件和小文件合并
    std::vector<std::vector<FileInfo> > fullBatches = create_batches(smallCandidates, BATCH_SIZE);
    
    // 轨道 B：大文件哈希极慢，每一个都拆成独立批次（Batch Size = 1），最大化动态负载均衡
    const std::vector<std::vector<FileInfo> > largeBatches = create_batches(partialCollisions, 1);
    fullBatches.insert(fullBatches.end(), largeBatches.begin(), largeBatches.end());

    const std::vector<HashResult> fullHashes = compute_full_hashes(fullBatches);

    // ==========================================
    // 展开物理折叠：将硬链接跟随者重新混入结果集
    // ==========================================
    std::vector<HashResult> unfoldedHashes = fullHashes;
    for (std::vector<HashResult>::const_iterator it = fullHashes.begin(); it != fullHashes.end(); ++it) {
        std::pair<uint64_t, uint64_t> devIno = std::make_pair(it->file.device, it->file.inode);
        std::map<std::pair<uint64_t, uint64_t>, std::vector<FileInfo> >::const_iterator followerIt = hardlinkFollowers.find(devIno);
        if (followerIt != hardlinkFollowers.end()) {
            for (std::vector<FileInfo>::const_iterator f = followerIt->second.begin(); f != followerIt->second.end(); ++f) {
                HashResult hr = *it;
                hr.file = *f;
                unfoldedHashes.push_back(hr);
            }
        }
    }

    // 滤掉最终防线
    const std::vector<DuplicateGroup> duplicateGroups = find_exact_duplicates(unfoldedHashes);

    DuplicateReport report = assemble_report(walkResult, partialHashes, unfoldedHashes, duplicateGroups);
    report.hashTasks = partialBatches.size() + fullBatches.size();
    return report;
}

std::vector<std::vector<FileInfo> > DuplicateFinder::create_batches(const std::vector<FileInfo>& candidates, size_t batchSize) const {
    std::vector<std::vector<FileInfo> > batches;
    if (batchSize == 0 || candidates.empty()) {
        return batches;
    }

    const size_t numBatches = (candidates.size() + batchSize - 1) / batchSize;
    batches.reserve(numBatches);

    for (size_t i = 0; i < candidates.size(); i += batchSize) {
        std::vector<FileInfo> batch;
        batch.reserve(batchSize);
        for (size_t j = i; j < i + batchSize && j < candidates.size(); ++j) {
            batch.push_back(candidates[j]);
        }
        batches.push_back(batch);
    }
    return batches;
}

std::vector<FileInfo> DuplicateFinder::find_size_collisions(const std::vector<FileInfo>& files) const {
    // 按文件分组
    std::map<uint64_t, std::vector<FileInfo> > filesBySize;
    for (std::vector<FileInfo>::const_iterator it = files.begin(); it != files.end(); ++it) {
        filesBySize[it->size].push_back(*it);
    }

    // 筛选过后的 vector<FileInfo>
    std::vector<FileInfo> collisions;
    for (std::map<uint64_t, std::vector<FileInfo> >::const_iterator it = filesBySize.begin(); it != filesBySize.end(); ++it) {
        if (it->second.size() < 2) {
            continue;
        }
        collisions.insert(collisions.end(), it->second.begin(), it->second.end());
    }
    return collisions;
}

void DuplicateFinder::split_candidates_by_threshold(const std::vector<FileInfo>& candidates, uint64_t threshold, std::vector<FileInfo>& smallOut, std::vector<FileInfo>& largeOut) const {
    for (std::vector<FileInfo>::const_iterator it = candidates.begin(); it != candidates.end(); ++it) {
        if (it->size <= threshold) {
            smallOut.push_back(*it);
        }
        else {
            largeOut.push_back(*it);
        }
    }
}

std::vector<HashResult> DuplicateFinder::compute_partial_hashes(const std::vector<std::vector<FileInfo> >& batches) const {
    std::vector<std::future<std::vector<HashResult> > > futures;
    futures.reserve(batches.size());

    for (std::vector<std::vector<FileInfo> >::const_iterator it = batches.begin(); it != batches.end(); ++it) {
        const std::vector<FileInfo> batch = *it;
        futures.push_back(pool_.submit_task([this, batch]() {
            std::vector<HashResult> batchResults;
            batchResults.reserve(batch.size());
            for (std::vector<FileInfo>::const_iterator fileIt = batch.begin(); fileIt != batch.end(); ++fileIt) {
                batchResults.push_back(hasher_.hash_partial_file(*fileIt, 4096)); // 4KiB partial hash
            }
            return batchResults;
            }));
    }

    std::vector<HashResult> results;
    size_t totalCandidates = 0;
    for (std::vector<std::vector<FileInfo> >::const_iterator it = batches.begin(); it != batches.end(); ++it) {
        totalCandidates += it->size();
    }
    results.reserve(totalCandidates);

    for (std::vector<std::future<std::vector<HashResult> > >::iterator it = futures.begin(); it != futures.end(); ++it) {
        std::vector<HashResult> batchResults = it->get();
        results.insert(results.end(), batchResults.begin(), batchResults.end());
    }
    return results;
}

std::vector<FileInfo> DuplicateFinder::find_partial_collisions(const std::vector<HashResult>& partialHashes) const {
    std::map<std::pair<uint64_t, Blake3Value>, std::vector<FileInfo> > buckets;
    for (std::vector<HashResult>::const_iterator it = partialHashes.begin(); it != partialHashes.end(); ++it) {
        if (it->ok) {
            buckets[std::make_pair(it->file.size, it->hash)].push_back(it->file);
        }
    }

    std::vector<FileInfo> collisions;
    for (std::map<std::pair<uint64_t, Blake3Value>, std::vector<FileInfo> >::const_iterator it = buckets.begin(); it != buckets.end(); ++it) {
        if (it->second.size() < 2) {
            continue;
        }
        collisions.insert(collisions.end(), it->second.begin(), it->second.end());
    }
    return collisions;
}

std::vector<HashResult> DuplicateFinder::compute_full_hashes(const std::vector<std::vector<FileInfo> >& batches) const {
    std::vector<std::future<std::vector<HashResult> > > futures;
    futures.reserve(batches.size());

    for (std::vector<std::vector<FileInfo> >::const_iterator it = batches.begin(); it != batches.end(); ++it) {
        const std::vector<FileInfo> batch = *it;
        futures.push_back(pool_.submit_task([this, batch]() {
            std::vector<HashResult> batchResults;
            batchResults.reserve(batch.size());
            for (std::vector<FileInfo>::const_iterator fileIt = batch.begin(); fileIt != batch.end(); ++fileIt) {
                batchResults.push_back(hasher_.hash_file(*fileIt));
            }
            return batchResults;
            }));
    }

    std::vector<HashResult> results;
    // Pre-allocate approximate capacity to avoid reallocations
    size_t totalCandidates = 0;
    for (std::vector<std::vector<FileInfo> >::const_iterator it = batches.begin(); it != batches.end(); ++it) {
        totalCandidates += it->size();
    }
    results.reserve(totalCandidates);

    for (std::vector<std::future<std::vector<HashResult> > >::iterator it = futures.begin(); it != futures.end(); ++it) {
        std::vector<HashResult> batchResults = it->get();
        results.insert(results.end(), batchResults.begin(), batchResults.end());
    }
    return results;
}

std::vector<DuplicateGroup> DuplicateFinder::find_exact_duplicates(const std::vector<HashResult>& fullHashes) const {
    std::map<std::pair<uint64_t, Blake3Value>, std::vector<std::string> > buckets;
    for (std::vector<HashResult>::const_iterator it = fullHashes.begin(); it != fullHashes.end(); ++it) {
        if (it->ok) {
            buckets[std::make_pair(it->file.size, it->hash)].push_back(it->file.path);
        }
    }

    std::vector<DuplicateGroup> groups;
    for (std::map<std::pair<uint64_t, Blake3Value>, std::vector<std::string> >::const_iterator it = buckets.begin(); it != buckets.end(); ++it) {
        if (it->second.size() > 1) {
            DuplicateGroup group;
            group.size = it->first.first;
            group.hash = it->first.second;
            group.paths = it->second;
            groups.push_back(group);
        }
    }
    return groups;
}

DuplicateReport DuplicateFinder::assemble_report(const FileWalkResult& walkResult, const std::vector<HashResult>& partialHashes, const std::vector<HashResult>& fullHashes, const std::vector<DuplicateGroup>& duplicateGroups) const {
    DuplicateReport report;
    report.scannedFiles = walkResult.files.size();
    report.errors.insert(report.errors.end(), walkResult.errors.begin(), walkResult.errors.end());

    for (std::vector<HashResult>::const_iterator it = partialHashes.begin(); it != partialHashes.end(); ++it) {
        if (!it->ok) {
            report.errors.push_back(it->error);
        }
        else {
            ++report.partialHashedFiles;
        }
    }

    for (std::vector<HashResult>::const_iterator it = fullHashes.begin(); it != fullHashes.end(); ++it) {
        if (!it->ok) {
            report.errors.push_back(it->error);
        }
        else {
            ++report.fullHashedFiles;
        }
    }

    report.duplicateGroups = duplicateGroups;

    for (std::vector<DuplicateGroup>::iterator it = report.duplicateGroups.begin(); it != report.duplicateGroups.end(); ++it) {
        std::sort(it->paths.begin(), it->paths.end());
    }

    std::sort(report.duplicateGroups.begin(), report.duplicateGroups.end(), [](const DuplicateGroup& left, const DuplicateGroup& right) {
        if (left.size != right.size) {
            return left.size < right.size;
        }
        if (left.hash != right.hash) {
            return left.hash < right.hash;
        }
        if (left.paths.empty() || right.paths.empty()) {
            return left.paths.size() < right.paths.size();
        }
        return left.paths[0] < right.paths[0];
        });

    std::sort(report.errors.begin(), report.errors.end(), [](const FileError& left, const FileError& right) {
        if (left.phase != right.phase) {
            return left.phase < right.phase;
        }
        return left.path < right.path;
        });

    return report;
}
