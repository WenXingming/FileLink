#include "StreamUploader.h"

#include <stdexcept>
#include <iomanip>
#include <sstream>
#include <spdlog/spdlog.h>

namespace filelink {

StreamUploader::StreamUploader(const std::string& tempFilePath)
    : tempFilePath_(tempFilePath) {

    // 打开文件进行二进制追加写入
    outStream_.open(tempFilePath_, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!outStream_.is_open()) {
        throw std::runtime_error("Failed to open temp file for StreamUploader: " + tempFilePath_);
    }

    // 初始化 BLAKE3 上下文
    blake3_hasher_init(&hasher_);
}

StreamUploader::~StreamUploader() {
    // 确保资源被清理
    if (outStream_.is_open()) {
        outStream_.close();
    }
}

void StreamUploader::appendChunk(const char* data, size_t length) {
    if (isFinalized_) {
        throw std::runtime_error("Cannot append chunk to a finalized StreamUploader");
    }
    if (length == 0 || data == nullptr) {
        spdlog::warn("StreamUploader::appendChunk called with empty data or null pointer. Ignoring this chunk.");
        return;
    }

    // 1. 计算哈希：将本块数据喂入增量哈希上下文中
    blake3_hasher_update(&hasher_, data, length);

    // 2. 落盘存底：写入本地临时文件
    outStream_.write(data, length);

    if (outStream_.bad()) {
        throw std::runtime_error("Failed to write chunk to temp file (possible disk full): " + tempFilePath_);
    }
}

std::string StreamUploader::finalize() {
    if (isFinalized_) {
        throw std::runtime_error("StreamUploader is already finalized");
    }

    // 显式刷新并关闭文件
    outStream_.flush();
    if (outStream_.bad()) {
        throw std::runtime_error("Failed to flush temp file: " + tempFilePath_);
    }
    outStream_.close();
    isFinalized_ = true;

    // 提取并生成最终的 64 位小写十六进制摘要
    uint8_t output[BLAKE3_OUT_LEN];
    blake3_hasher_finalize(&hasher_, output, BLAKE3_OUT_LEN);

    std::ostringstream hexStream;
    hexStream << std::hex << std::setfill('0');
    for (int i = 0; i < BLAKE3_OUT_LEN; ++i) {
        hexStream << std::setw(2) << static_cast<int>(output[i]);
    }

    return hexStream.str();
}

} // namespace filelink
