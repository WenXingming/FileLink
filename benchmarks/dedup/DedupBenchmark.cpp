#include <benchmark/benchmark.h>

#include "DuplicateFinder.h"
#include "ThreadPool.h"

#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {

struct DatasetConfig {
    int fileCount;
    int fileSize;
    double duplicateRatio;
    bool sameSize;
    bool sharedPrefix;
};

std::string make_temp_dir() {
    std::string pattern = "/tmp/threadpool_dedup_bench_XXXXXX";
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');

    char* path = mkdtemp(buffer.data());
    if (path == nullptr) {
        throw std::runtime_error("mkdtemp failed");
    }
    return std::string(path);
}

std::string make_content(const std::string& seed, int size) {
    std::string content;
    content.reserve(static_cast<size_t>(size));

    while (static_cast<int>(content.size()) < size) {
        content += seed;
        content += "|";
    }
    content.resize(static_cast<size_t>(size));
    return content;
}

void write_file(const std::string& path, const std::string& content) {
    std::ofstream output(path.c_str(), std::ios::binary);
    if (!output) {
        throw std::runtime_error("failed to create benchmark file");
    }
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
}

void generate_dataset(const std::string& root, const DatasetConfig& config, std::vector<std::string>& files) {
    if (config.fileSize == -1) {
        for (int i = 0; i < config.fileCount; ++i) {
            int actualSize;
            if (i % 10 == 0) actualSize = 8 * 1024 * 1024; // 10% large (8MiB)
            else if (i % 3 == 0) actualSize = 1 * 1024 * 1024; // ~30% medium (1MiB)
            else actualSize = 4 * 1024; // rest small (4KiB)
            
            bool isDuplicate = (i % 5 == 0); // 20% duplicates
            std::string path = root + "/mixed_" + std::to_string(i) + ".dat";
            std::string content = make_content(isDuplicate ? "mixed-dup-" + std::to_string(i % 10) : "mixed-uniq-" + std::to_string(i), actualSize);
            write_file(path, content);
            files.push_back(path);
        }
        return;
    }

    const int duplicateFiles = static_cast<int>(config.fileCount * config.duplicateRatio) & ~1;
    files.reserve(static_cast<size_t>(config.fileCount));

    std::string sharedContent;
    if (config.sharedPrefix) {
        sharedContent = make_content("shared-massive-prefix", config.fileSize);
    }

    for (int i = 0; i < duplicateFiles; ++i) {
        const std::string path = root + "/duplicate_" + std::to_string(i) + ".dat";
        std::string content = config.sharedPrefix ? sharedContent : make_content("duplicate-group-" + std::to_string(i / 2), config.fileSize);
        if (config.sharedPrefix) {
            std::string tail = "duplicate-group-" + std::to_string(i / 2);
            if (content.size() > tail.size()) content.replace(content.size() - tail.size(), tail.size(), tail);
        }
        write_file(path, content);
        files.push_back(path);
    }

    for (int i = duplicateFiles; i < config.fileCount; ++i) {
        const std::string path = root + "/unique_" + std::to_string(i) + ".dat";
        int actualSize = config.fileSize;
        if (!config.sameSize) {
            actualSize += i; // Perturb size to avoid size collision
        }
        std::string content = config.sharedPrefix ? sharedContent : make_content("unique-file-" + std::to_string(i), actualSize);
        if (config.sharedPrefix) {
            std::string tail = "unique-file-" + std::to_string(i);
            if (content.size() > tail.size()) content.replace(content.size() - tail.size(), tail.size(), tail);
        }
        write_file(path, content);
        files.push_back(path);
    }
}

void remove_dataset(const std::string& root, const std::vector<std::string>& files) {
    for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++it) {
        std::remove(it->c_str());
    }
    rmdir(root.c_str());
}

void run_dedup_benchmark(benchmark::State& state) {
    DatasetConfig config;
    config.fileCount = static_cast<int>(state.range(0));
    config.fileSize = static_cast<int>(state.range(1));
    const int threadCount = static_cast<int>(state.range(2));
    config.duplicateRatio = static_cast<double>(state.range(3)) / 100.0;
    config.sameSize = state.range(4) != 0;
    config.sharedPrefix = state.range(5) != 0;

    std::string root;
    std::vector<std::string> files;

    wxm::ThreadPool pool(threadCount, threadCount * 4, false, 1000);
    FileWalker walker;
    Hasher hasher;
    const DuplicateFinder finder(pool, walker, hasher);

    for (auto _ : state) {
        if (root.empty()) {
            state.PauseTiming();
            root = make_temp_dir();
            generate_dataset(root, config, files);
            state.ResumeTiming();
        }

        const DuplicateReport report = finder.find_duplicates(root);
        benchmark::DoNotOptimize(report.scannedFiles);
        benchmark::DoNotOptimize(report.fullHashedFiles);
        benchmark::DoNotOptimize(report.duplicateGroups.size());

        if (!report.errors.empty()) {
            state.SkipWithError("dedup reported errors");
            break;
        }
    }

    state.PauseTiming();
    if (!root.empty()) {
        remove_dataset(root, files);
    }
    state.SetItemsProcessed(state.iterations() * config.fileCount);
    if (config.fileSize != -1) {
        state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(config.fileCount) * config.fileSize);
    }
}

void register_workload(const std::string& name, int fileCount, int fileSize, int duplicatePercent, bool sameSize, bool sharedPrefix = false) {
    const int threadCounts[] = { 1, 2, 4, 8 };
    for (size_t i = 0; i < sizeof(threadCounts) / sizeof(threadCounts[0]); ++i) {
        const int threadCount = threadCounts[i];
        const std::string benchmarkName = "DuplicateFinder/" + name + "/threads_" + std::to_string(threadCount);
        benchmark::RegisterBenchmark(benchmarkName.c_str(), run_dedup_benchmark)
            ->Args({ fileCount, fileSize, threadCount, duplicatePercent, sameSize ? 1 : 0, sharedPrefix ? 1 : 0 })
            ->Unit(benchmark::kMillisecond);
    }
}

void register_benchmarks() {
    // Basic baseline workloads
    register_workload("100_files_4KiB", 100, 4 * 1024, 20, false);
    register_workload("1000_files_4KiB", 1000, 4 * 1024, 20, false);
    register_workload("10000_files_4KiB", 10000, 4 * 1024, 20, false);
    register_workload("1000_files_256KiB", 1000, 256 * 1024, 20, false);

    // 1. Small file overhead scenarios
    register_workload("many_small_unique_same_size", 10000, 4 * 1024, 0, true);
    register_workload("many_small_duplicates", 10000, 4 * 1024, 100, true);

    // 2. Large file partial hash scenarios (20 files * 8MiB)
    register_workload("large_same_size_different_content", 20, 8 * 1024 * 1024, 0, true);
    register_workload("large_same_size_same_prefix_different_tail", 20, 8 * 1024 * 1024, 0, true, true);
    register_workload("large_duplicates", 20, 8 * 1024 * 1024, 100, true);

    // 3. Mixed realistic scenario (1000 files: ~4GiB total)
    register_workload("mixed_realistic", 1000, -1, 20, false);
}

} // namespace

int main(int argc, char** argv) {
    register_benchmarks();
    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }
    benchmark::RunSpecifiedBenchmarks();
    return 0;
}
