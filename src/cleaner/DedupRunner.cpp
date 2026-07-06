#include "DedupRunner.h"
#include "DuplicateFinder.h"
#include "ThreadPool.h"
#include "FileWalker.h"
#include "Hasher.h"
#include <unistd.h>
#include <sys/stat.h>
#include <iostream>
#include <cerrno>
#include <cstring>
#include <vector>

namespace filelink {

DedupRunner::DedupRunner(std::string storageRoot)
    : storageRoot_(std::move(storageRoot)) {
}

int DedupRunner::run_dedup() {
    std::string objectsDir = storageRoot_ + "/objects";
    
    // 确保扫描目录存在
    struct stat st;
    if (::stat(objectsDir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
        std::cerr << "[Dedup] Target objects directory does not exist: " << objectsDir << "\n";
        return 0;
    }

    // 初始化去重引擎的线程池、walker 和 hasher
    // 限制去重扫描的线程数为 2，以防在大吞吐下给磁盘造成过度负担
    wxm::ThreadPool pool(2, 8, false, 1000);
    FileWalker walker;
    Hasher hasher;
    DuplicateFinder finder(pool, walker, hasher);

    std::cout << "[Dedup] Starting offline deduplication scanning under: " << objectsDir << " ...\n";
    DuplicateReport report = finder.find_duplicates(objectsDir);

    int mergedFilesCount = 0;
    if (report.duplicateGroups.empty()) {
        std::cout << "[Dedup] No duplicate files found.\n";
        return 0;
    }

    std::cout << "[Dedup] Found " << report.duplicateGroups.size() << " duplicate groups. Commencing physical merging...\n";

    for (const auto& group : report.duplicateGroups) {
        if (group.paths.size() < 2) {
            continue;
        }

        const std::string& anchorPath = group.paths[0];

        // 遍历组内其余文件，用硬链接指向 anchorPath 进行原子覆盖
        for (size_t i = 1; i < group.paths.size(); ++i) {
            const std::string& targetPath = group.paths[i];
            
            // 为了防止同文件自身的意外 link 错误 (比如路径字符相同的情况，虽然 finder 会排除，但防御性校验依然必要)
            struct stat anchorSt, targetSt;
            if (::stat(anchorPath.c_str(), &anchorSt) != 0 || ::stat(targetPath.c_str(), &targetSt) != 0) {
                continue;
            }
            if (anchorSt.st_dev == targetSt.st_dev && anchorSt.st_ino == targetSt.st_ino) {
                // 已经是同一个 inode 物理文件，无需再次合并
                continue;
            }

            std::string tempPath = targetPath + ".tmp_dedup";
            ::unlink(tempPath.c_str()); // 清除残留

            // 建立硬链接指向 anchor
            if (::link(anchorPath.c_str(), tempPath.c_str()) != 0) {
                std::cerr << "[Dedup] Failed to link " << anchorPath << " to " << tempPath 
                          << ", error: " << std::strerror(errno) << "\n";
                continue;
            }

            // 原子替换原来的重复文件
            if (::rename(tempPath.c_str(), targetPath.c_str()) != 0) {
                std::cerr << "[Dedup] Failed to atomic rename " << tempPath << " to " << targetPath 
                          << ", error: " << std::strerror(errno) << "\n";
                ::unlink(tempPath.c_str());
                continue;
            }

            mergedFilesCount++;
        }
    }

    std::cout << "[Dedup] Deduplication completed. Merged " << mergedFilesCount << " redundant files.\n";
    return mergedFilesCount;
}

} // namespace filelink
