#include "Hasher.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {

std::string make_temp_dir() {
    std::string pattern = "/tmp/threadpool_hasher_test_XXXXXX";
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');

    char* path = mkdtemp(buffer.data());
    if (path == nullptr) {
        throw std::runtime_error("mkdtemp failed");
    }
    return std::string(path);
}

void write_file(const std::string& path, const std::string& content) {
    std::ofstream output(path.c_str(), std::ios::binary);
    output << content;
}

void remove_file_tree(const std::string& root, const std::vector<std::string>& files) {
    for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++it) {
        std::remove(it->c_str());
    }
    rmdir(root.c_str());
}

FileInfo file_info(const std::string& path) {
    struct stat status;
    if (stat(path.c_str(), &status) != 0) {
        return FileInfo{ path, 0, 0, 0 };
    }
    return FileInfo{ path, static_cast<uint64_t>(status.st_size), static_cast<uint64_t>(status.st_dev), static_cast<uint64_t>(status.st_ino) };
}

} // namespace

TEST(HasherTest, SameContentHasSameHash) {
    const std::string root = make_temp_dir();
    const std::string first = root + "/first.txt";
    const std::string second = root + "/second.txt";
    write_file(first, "same content");
    write_file(second, "same content");

    const Hasher hasher;
    const HashResult firstHash = hasher.hash_file(file_info(first));
    const HashResult secondHash = hasher.hash_file(file_info(second));

    ASSERT_TRUE(firstHash.ok);
    ASSERT_TRUE(secondHash.ok);
    EXPECT_EQ(firstHash.hash, secondHash.hash);

    remove_file_tree(root, std::vector<std::string>{ first, second });
}

TEST(HasherTest, DifferentContentHasDifferentHash) {
    const std::string root = make_temp_dir();
    const std::string first = root + "/first.txt";
    const std::string second = root + "/second.txt";
    write_file(first, "alpha");
    write_file(second, "bravo");

    const Hasher hasher;
    const HashResult firstHash = hasher.hash_file(file_info(first));
    const HashResult secondHash = hasher.hash_file(file_info(second));

    ASSERT_TRUE(firstHash.ok);
    ASSERT_TRUE(secondHash.ok);
    EXPECT_NE(firstHash.hash, secondHash.hash);

    remove_file_tree(root, std::vector<std::string>{ first, second });
}

TEST(HasherTest, EmptyFileCanBeHashed) {
    const std::string root = make_temp_dir();
    const std::string file = root + "/empty.bin";
    write_file(file, "");

    const Hasher hasher;
    const HashResult result = hasher.hash_file(file_info(file));

    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.error.message, "");

    remove_file_tree(root, std::vector<std::string>{ file });
}

TEST(HasherTest, MissingFileReturnsError) {
    const Hasher hasher;

    const HashResult result = hasher.hash_file(FileInfo{ "/tmp/threadpool_hasher_missing_file_for_test", 0, 0, 0 });

    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.error.message.empty());
}

TEST(HasherTest, SmallBufferMatchesDefaultBuffer) {
    const std::string root = make_temp_dir();
    const std::string file = root + "/long.txt";
    write_file(file, "abcdefghijklmnopqrstuvwxyz0123456789");

    const Hasher defaultHasher;
    const Hasher smallBufferHasher(3);
    const FileInfo info = file_info(file);

    const HashResult defaultResult = defaultHasher.hash_file(info);
    const HashResult smallBufferResult = smallBufferHasher.hash_file(info);

    ASSERT_TRUE(defaultResult.ok);
    ASSERT_TRUE(smallBufferResult.ok);
    EXPECT_EQ(defaultResult.hash, smallBufferResult.hash);

    remove_file_tree(root, std::vector<std::string>{ file });
}

TEST(HasherTest, PartialHashMatchesFullHashIfMaxBytesIsLarge) {
    const std::string root = make_temp_dir();
    const std::string file = root + "/test.txt";
    write_file(file, "short content");

    const Hasher hasher;
    const FileInfo info = file_info(file);

    const HashResult fullResult = hasher.hash_file(info);
    const HashResult partialResult = hasher.hash_partial_file(info, 1024); // maxBytes > size

    ASSERT_TRUE(fullResult.ok);
    ASSERT_TRUE(partialResult.ok);
    EXPECT_EQ(fullResult.hash, partialResult.hash);

    remove_file_tree(root, std::vector<std::string>{ file });
}

TEST(HasherTest, PartialHashReadsHeadAndTailIfMaxBytesIsSmall) {
    const std::string root = make_temp_dir();
    const std::string file = root + "/test.txt";
    // length of this string is 35
    write_file(file, "long content that will be truncated");

    // maxBytes = 4, chunkSize = 2
    // Head 2 bytes: "lo"
    // Tail 2 bytes: "ed"
    const std::string expectedStr = "loed";
    const std::string expectedFile = root + "/expected.txt";
    write_file(expectedFile, expectedStr);

    const Hasher hasher;
    const FileInfo info = file_info(file);
    const FileInfo expectedInfo = file_info(expectedFile);

    const HashResult fullResult = hasher.hash_file(info);
    const HashResult partialResult = hasher.hash_partial_file(info, 4); // maxBytes = 4
    const HashResult expectedResult = hasher.hash_file(expectedInfo);

    ASSERT_TRUE(fullResult.ok);
    ASSERT_TRUE(partialResult.ok);
    ASSERT_TRUE(expectedResult.ok);
    
    EXPECT_NE(fullResult.hash, partialResult.hash); // Should be different from full
    EXPECT_EQ(partialResult.hash, expectedResult.hash); // Should perfectly match the head+tail concatenation

    remove_file_tree(root, std::vector<std::string>{ file, expectedFile });
}

TEST(HasherTest, PartialHashIgnoresMiddleDifferences) {
    const std::string root = make_temp_dir();
    const std::string fileA = root + "/fileA.txt";
    const std::string fileB = root + "/fileB.txt";
    
    // 我们设定 maxBytes 为 10，那么 chunkSize 是 5。
    // 首尾各 5 字节，中间不同。
    // fileA: [12345] [AAAAAAAA] [67890]
    // fileB: [12345] [BBBBBBBB] [67890]
    std::string head = "12345";
    std::string tail = "67890";
    write_file(fileA, head + "AAAAAAAA" + tail);
    write_file(fileB, head + "BBBBBBBB" + tail);

    const Hasher hasher;
    const HashResult resultA = hasher.hash_partial_file(file_info(fileA), 10);
    const HashResult resultB = hasher.hash_partial_file(file_info(fileB), 10);

    ASSERT_TRUE(resultA.ok);
    ASSERT_TRUE(resultB.ok);
    
    // 首尾完全一致，中间的差异被成功忽略，局部哈希应当相同！
    EXPECT_EQ(resultA.hash, resultB.hash);

    remove_file_tree(root, std::vector<std::string>{ fileA, fileB });
}

TEST(HasherTest, PartialHashCatchesTailDifferences) {
    const std::string root = make_temp_dir();
    const std::string fileA = root + "/fileA.txt";
    const std::string fileB = root + "/fileB.txt";
    
    // 前面和中间完全一样，但是尾巴截然不同。
    // fileA: [12345] [CCCCCCCC] [abcde]
    // fileB: [12345] [CCCCCCCC] [vwxyz]
    std::string head = "12345";
    std::string middle = "CCCCCCCC";
    write_file(fileA, head + middle + "abcde");
    write_file(fileB, head + middle + "vwxyz");

    const Hasher hasher;
    const HashResult resultA = hasher.hash_partial_file(file_info(fileA), 10);
    const HashResult resultB = hasher.hash_partial_file(file_info(fileB), 10);

    ASSERT_TRUE(resultA.ok);
    ASSERT_TRUE(resultB.ok);
    
    // 尽管前缀完全一样，由于尾部有微小差异，局部哈希应当完全不同！
    EXPECT_NE(resultA.hash, resultB.hash);

    remove_file_tree(root, std::vector<std::string>{ fileA, fileB });
}
