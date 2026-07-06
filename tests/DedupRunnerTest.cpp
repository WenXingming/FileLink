#include <gtest/gtest.h>
#include "cleaner/DedupRunner.h"
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

using namespace filelink;

class DedupRunnerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建测试目录结构
        testStorage = "./storage_test_dedup";
        objectsDir = testStorage + "/objects";
        
        ::mkdir(testStorage.c_str(), 0755);
        ::mkdir(objectsDir.c_str(), 0755);
    }

    void TearDown() override {
        // 清理测试生成的文件与文件夹
        std::string file1 = objectsDir + "/file1.bin";
        std::string file2 = objectsDir + "/file2.bin";
        ::unlink(file1.c_str());
        ::unlink(file2.c_str());
        ::rmdir(objectsDir.c_str());
        ::rmdir(testStorage.c_str());
    }

    std::string testStorage;
    std::string objectsDir;
};

TEST_F(DedupRunnerTest, DedupMergesIdenticalFilesToHardLinks) {
    std::string file1 = objectsDir + "/file1.bin";
    std::string file2 = objectsDir + "/file2.bin";

    // 1. 创建两个内容完全一致的物理文件
    std::string content = "shared_identical_data_content_12345";
    
    std::ofstream ofs1(file1, std::ios::binary);
    ofs1 << content;
    ofs1.close();

    std::ofstream ofs2(file2, std::ios::binary);
    ofs2 << content;
    ofs2.close();

    // 验证初始状态：硬链接数均为 1，且 inode 不同
    struct stat st1, st2;
    ASSERT_EQ(::stat(file1.c_str(), &st1), 0);
    ASSERT_EQ(::stat(file2.c_str(), &st2), 0);
    EXPECT_EQ(st1.st_nlink, 1);
    EXPECT_EQ(st2.st_nlink, 1);
    EXPECT_NE(st1.st_ino, st2.st_ino);

    // 2. 执行去重
    DedupRunner runner(testStorage);
    int merged = runner.run_dedup();
    EXPECT_EQ(merged, 1);

    // 3. 校验结果
    // 校验内容是否无损
    std::ifstream ifs1(file1, std::ios::binary);
    std::string readContent1((std::istreambuf_iterator<char>(ifs1)), std::istreambuf_iterator<char>());
    EXPECT_EQ(readContent1, content);

    std::ifstream ifs2(file2, std::ios::binary);
    std::string readContent2((std::istreambuf_iterator<char>(ifs2)), std::istreambuf_iterator<char>());
    EXPECT_EQ(readContent2, content);

    // 校验硬链接特性：物理合并
    ASSERT_EQ(::stat(file1.c_str(), &st1), 0);
    ASSERT_EQ(::stat(file2.c_str(), &st2), 0);
    EXPECT_EQ(st1.st_nlink, 2);
    EXPECT_EQ(st2.st_nlink, 2);
    EXPECT_EQ(st1.st_ino, st2.st_ino); // 应该是同一个 inode 号了
}
