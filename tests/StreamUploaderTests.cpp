#include "StreamUploader.h"
#include "LocalObjectStore.h"

#include <gtest/gtest.h>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

namespace {

bool path_exists(const std::string& path) {
    struct stat info;
    return ::stat(path.c_str(), &info) == 0;
}

std::string read_file(const std::string& path) {
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file.is_open()) return "";
    std::string content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    return content;
}

} // namespace

class StreamUploaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::string pattern = "/tmp/filelink_stream_uploader_XXXXXX";
        std::vector<char> buffer(pattern.begin(), pattern.end());
        buffer.push_back('\0');

        char* path = ::mkdtemp(buffer.data());
        ASSERT_NE(path, nullptr);
        testDir_ = path;
    }

    void TearDown() override {
        // Simple manual cleanup
        std::remove((testDir_ + "/temp_upload_1.tmp").c_str());
        std::remove((testDir_ + "/temp_upload_2.tmp").c_str());
        std::remove((testDir_ + "/temp_upload_3.tmp").c_str());
        
        // Remove object dir parts if created by test
        std::remove((testDir_ + "/objects/d7/49/d74981efa70a0c880b8d8c1985d075dbcbf679b99a5f9914e5aaf96b831a9e24").c_str());
        std::remove((testDir_ + "/objects/d7/49").c_str());
        std::remove((testDir_ + "/objects/d7").c_str());
        std::remove((testDir_ + "/objects").c_str());
        
        std::remove(testDir_.c_str());
    }

    std::string testDir_;
};

// 验证流式追加多个数据块后，文件内容正确且哈希符合预期
TEST_F(StreamUploaderTest, ComputesCorrectHashAndWritesContent) {
    std::string tempFile = testDir_ + "/temp_upload_1.tmp";
    
    // "hello world" 的 BLAKE3 官方测试向量（长度 32 字节，HEX 小写）
    const std::string expectedHash = "d74981efa70a0c880b8d8c1985d075dbcbf679b99a5f9914e5aaf96b831a9e24"; 
    
    filelink::StreamUploader uploader(tempFile);
    
    // 分批次喂入数据流，模拟网络分块到达
    uploader.appendChunk("hello ", 6);
    uploader.appendChunk("world", 5);
    
    // 验证返回的哈希是否正确
    std::string actualHash = uploader.finalize();
    EXPECT_EQ(actualHash, expectedHash);
    
    // 验证文件是否成功落盘并且内容一致
    EXPECT_TRUE(path_exists(tempFile));
    EXPECT_EQ(read_file(tempFile), "hello world");
}

// 验证抛出异常的边界情况
TEST_F(StreamUploaderTest, ThrowsWhenFinalizedTwice) {
    std::string tempFile = testDir_ + "/temp_upload_2.tmp";
    filelink::StreamUploader uploader(tempFile);
    uploader.appendChunk("data", 4);
    
    uploader.finalize();
    
    // 再次调用应该抛出异常，防止重复操作
    EXPECT_THROW(uploader.finalize(), std::runtime_error);
    EXPECT_THROW(uploader.appendChunk("more", 4), std::runtime_error);
}

// 验证不能打开文件时的行为
TEST_F(StreamUploaderTest, ThrowsWhenFileCannotBeOpened) {
    // 目录不可直接作为文件进行打开写入
    std::string invalidFile = testDir_;
    
    EXPECT_THROW({ filelink::StreamUploader uploader(invalidFile); }, std::runtime_error);
}

// （进阶集成测试）验证与 LocalObjectStore 的协同闭环
TEST_F(StreamUploaderTest, IntegratesWithLocalObjectStore) {
    std::string tempFile = testDir_ + "/temp_upload_3.tmp";
    std::string storageRoot = testDir_;
    
    // 1. 使用 StreamUploader 边收边算
    filelink::StreamUploader uploader(tempFile);
    std::string content = "Integration Testing Content";
    // 注意：不能简单使用 "Integration Testing Content" 因为它的哈希不是 expectedHash (d74981ef...)
    // 为了让 teardown 中的 remove 起作用，我们用 "hello world"
    content = "hello world";
    uploader.appendChunk(content.c_str(), content.length());
    std::string finalHash = uploader.finalize();
    
    // 临时文件此刻应当存在
    EXPECT_TRUE(path_exists(tempFile));
    
    // 2. 将临时文件提交给底层对象存储
    filelink::LocalObjectStore store(storageRoot);
    auto result = store.commit(tempFile, finalHash);
    
    EXPECT_EQ(result.status, filelink::CommitStatus::Created);
    
    // 3. 验证提交结果：临时文件被清空（unlink），对象库里正确就位
    EXPECT_FALSE(path_exists(tempFile));
    
    // 根据两级目录规则：objects/前两位/次两位/完整哈希
    std::string objectPath = storageRoot + "/objects/" + finalHash.substr(0, 2) + "/" + finalHash.substr(2, 2) + "/" + finalHash;
    EXPECT_TRUE(path_exists(objectPath));
    EXPECT_EQ(read_file(objectPath), content);
}
