#include "storage/LocalObjectStore.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace {

const std::string kHash(64, 'a');

bool path_exists(const std::string& path) {
    struct stat info;
    return ::stat(path.c_str(), &info) == 0;
}

void write_file(const std::string& path, const std::string& content) {
    std::ofstream output(path.c_str(), std::ios::binary);
    ASSERT_TRUE(output.is_open());
    output << content;
}

std::string read_file(const std::string& path) {
    std::ifstream input(path.c_str(), std::ios::binary);
    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

class LocalObjectStoreTest : public testing::Test {
protected:
    void SetUp() override {
        std::string pattern = "/tmp/filelink_object_store_XXXXXX";
        std::vector<char> buffer(pattern.begin(), pattern.end());
        buffer.push_back('\0');

        char* path = ::mkdtemp(buffer.data());
        ASSERT_NE(path, nullptr);
        baseDir_ = path;
        storageRoot_ = baseDir_ + "/storage";
    }

    void TearDown() override {
        std::remove((storageRoot_ + "/objects/aa/aa/" + kHash).c_str());
        std::remove((storageRoot_ + "/objects/aa/aa").c_str());
        std::remove((storageRoot_ + "/objects/aa").c_str());
        std::remove((storageRoot_ + "/objects").c_str());
        std::remove((baseDir_ + "/first.tmp").c_str());
        std::remove((baseDir_ + "/second.tmp").c_str());
        std::remove(storageRoot_.c_str());
        std::remove(baseDir_.c_str());
    }

    std::string object_path() const {
        return storageRoot_ + "/objects/aa/aa/" + kHash;
    }

    std::string baseDir_;
    std::string storageRoot_;
};

} // namespace

TEST_F(LocalObjectStoreTest, CreatesContentAddressedObject) {
    const std::string tempPath = baseDir_ + "/first.tmp";
    write_file(tempPath, "first content");

    const filelink::LocalObjectStore store(storageRoot_);
    const filelink::CommitResult result = store.commit(tempPath, kHash);

    EXPECT_EQ(result.status, filelink::CommitStatus::Created);
    EXPECT_EQ(result.objectPath, object_path());
    EXPECT_FALSE(path_exists(tempPath));
    EXPECT_EQ(read_file(result.objectPath), "first content");
}

TEST_F(LocalObjectStoreTest, ReusesExistingObjectWithoutOverwritingIt) {
    const std::string firstTemp = baseDir_ + "/first.tmp";
    const std::string secondTemp = baseDir_ + "/second.tmp";
    write_file(firstTemp, "original content");
    write_file(secondTemp, "different content");

    const filelink::LocalObjectStore store(storageRoot_);
    ASSERT_EQ(store.commit(firstTemp, kHash).status, filelink::CommitStatus::Created);

    const filelink::CommitResult result = store.commit(secondTemp, kHash);

    EXPECT_EQ(result.status, filelink::CommitStatus::Reused);
    EXPECT_EQ(result.objectPath, object_path());
    EXPECT_FALSE(path_exists(secondTemp));
    EXPECT_EQ(read_file(result.objectPath), "original content");
}

TEST_F(LocalObjectStoreTest, RejectsInvalidHashAndPreservesTempFile) {
    const std::string tempPath = baseDir_ + "/first.tmp";
    write_file(tempPath, "content");

    const filelink::LocalObjectStore store(storageRoot_);

    EXPECT_THROW(store.commit(tempPath, std::string(64, 'A')), std::invalid_argument);
    EXPECT_TRUE(path_exists(tempPath));
    EXPECT_FALSE(path_exists(storageRoot_));
}

TEST_F(LocalObjectStoreTest, PreservesTempFileWhenObjectDirectoryCannotBeCreated) {
    const std::string tempPath = baseDir_ + "/first.tmp";
    write_file(storageRoot_, "not a directory");
    write_file(tempPath, "content");

    const filelink::LocalObjectStore store(storageRoot_);

    EXPECT_THROW(store.commit(tempPath, kHash), std::system_error);
    EXPECT_TRUE(path_exists(tempPath));
}
