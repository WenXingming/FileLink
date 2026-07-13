#pragma once

#include <string>

namespace filelink {

// ====================================================================
// DedupRunner：离线扫描对象目录，并用硬链接合并重复物理文件。
// ====================================================================
class DedupRunner {
public:
    explicit DedupRunner(std::string storageRoot);
    ~DedupRunner() = default;

    DedupRunner(const DedupRunner&) = delete;
    DedupRunner& operator=(const DedupRunner&) = delete;

    int run_dedup();

private:
    std::string storageRoot_;
};

} // namespace filelink
