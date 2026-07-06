#pragma once

#include <string>

namespace filelink {

class DedupRunner {
public:
    explicit DedupRunner(std::string storageRoot);
    ~DedupRunner() = default;

    DedupRunner(const DedupRunner&) = delete;
    DedupRunner& operator=(const DedupRunner&) = delete;

    /**
     * @brief 扫描存储目录并对重复物理文件进行原子硬链接替换
     * @return 成功合并替换的重复文件副本数量
     */
    int run_dedup();

private:
    std::string storageRoot_;
};

} // namespace filelink
