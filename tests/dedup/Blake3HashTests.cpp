#include <gtest/gtest.h>
#include "Blake3Hash.h"

TEST(Blake3HashTests, BasicHashing) {
    Blake3Hash hasher;
    std::string data = "hello world";
    hasher.update(data.data(), data.size());
    Blake3Value hash_val = hasher.value();
    
    // Check length
    EXPECT_EQ(hash_val.bytes.size(), BLAKE3_OUT_LEN);

    // Check string representation is non-empty and 64 characters long (32 bytes = 64 hex chars)
    std::string hex_str = hash_val.to_string();
    EXPECT_EQ(hex_str.length(), 64);
    
    // Ensure deterministic hashing
    Blake3Hash hasher2;
    hasher2.update(data.data(), data.size());
    Blake3Value hash_val2 = hasher2.value();
    
    EXPECT_EQ(hash_val, hash_val2);
    EXPECT_EQ(hex_str, hash_val2.to_string());
}

TEST(Blake3HashTests, Inequality) {
    Blake3Hash h1;
    std::string data1 = "hello";
    h1.update(data1.data(), data1.size());
    
    Blake3Hash h2;
    std::string data2 = "world";
    h2.update(data2.data(), data2.size());
    
    EXPECT_NE(h1.value(), h2.value());
}
