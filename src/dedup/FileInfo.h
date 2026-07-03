#pragma once

#include <cstdint>
#include <string>

struct FileInfo {
    std::string path;
    uint64_t size;
    uint64_t device;
    uint64_t inode;
};

struct FileError {
    enum class Phase {
        SCAN,
        HASH
    };
    Phase phase;
    std::string path;
    std::string message;
};
