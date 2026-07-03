#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

#include "blake3.h"

// 256-bit (32-byte) Hash Value
struct Blake3Value {
    std::array<uint8_t, BLAKE3_OUT_LEN> bytes;

    bool operator==(const Blake3Value& other) const {
        return std::memcmp(bytes.data(), other.bytes.data(), BLAKE3_OUT_LEN) == 0;
    }

    bool operator!=(const Blake3Value& other) const {
        return !(*this == other);
    }

    bool operator<(const Blake3Value& other) const {
        return std::memcmp(bytes.data(), other.bytes.data(), BLAKE3_OUT_LEN) < 0;
    }

    std::string to_string() const {
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (uint8_t b : bytes) {
            ss << std::setw(2) << static_cast<int>(b);
        }
        return ss.str();
    }
};

class Blake3Hash {
public:
    Blake3Hash() {
        blake3_hasher_init(&hasher_);
    }

    void update(const void* data, size_t size) {
        blake3_hasher_update(&hasher_, data, size);
    }

    Blake3Value value() const {
        Blake3Value val;
        blake3_hasher_finalize(&hasher_, val.bytes.data(), BLAKE3_OUT_LEN);
        return val;
    }

private:
    blake3_hasher hasher_;
};
