#pragma once

#include <string>
#include <fstream>
#include "blake3.h"

namespace filelink {

/**
 * @brief 流式大文件接收器
 *
 * 负责在网络接收流数据的同时：
 * 1. 实时计算 BLAKE3 哈希（避免落盘后再重新读取全文件的 IO 开销）。
 * 2. 实时将数据写入指定的本地临时文件。
 * 当接收完成后，返回最终的 64 位小写十六进制哈希摘要。
 */
class StreamUploader {
public:

    explicit StreamUploader(const std::string& tempFilePath);
    ~StreamUploader();

    // 禁用拷贝和赋值
    StreamUploader(const StreamUploader&) = delete;
    StreamUploader& operator=(const StreamUploader&) = delete;

    void appendChunk(const char* data, size_t length);
    std::string finalize();

private:
    std::string tempFilePath_;
    std::ofstream outStream_;
    blake3_hasher hasher_;
    bool isFinalized_{ false };
};

} // namespace filelink
