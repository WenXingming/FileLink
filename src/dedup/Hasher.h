#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "FileInfo.h"
#include "Blake3Hash.h"

struct HashResult {
    bool ok;
    FileInfo file;
    Blake3Value hash;
    FileError error;
};

class Hasher {
public:
    explicit Hasher(size_t bufferSize = 1024 * 1024);

    HashResult hash_file(const FileInfo& file) const;
    HashResult hash_partial_file(const FileInfo& file, size_t maxBytes) const;

private:
    size_t bufferSize_;
};
