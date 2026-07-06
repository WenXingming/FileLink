#pragma once

#include "ObjectStore.h"
#include "blake3.h"
#include <string>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <chrono>

namespace soci {
class connection_pool;
}

namespace filelink {

struct ActiveHasher {
    blake3_hasher hasher;
    uint64_t current_offset = 0;
    std::chrono::steady_clock::time_point last_active;
};

enum class UploadChunkResult {
    Success,
    SessionNotFound,
    OffsetMismatch,
    InvalidChunkSize,
    SystemError
};

namespace db {
struct UploadSession;
}

class UploadService {
public:
    UploadService(soci::connection_pool& pool, std::string storageRoot, ObjectStore store);
    ~UploadService() = default;

    UploadService(const UploadService&) = delete;
    UploadService& operator=(const UploadService&) = delete;

    /**
     * @brief 获取上传进度
     * @param uploadIdHex 16进制的会话ID
     * @param out_offset 输出当前偏移量
     * @param out_totalSize 输出总大小
     * @return 是否成功找到会话
     */
    bool get_session_progress(const std::string& uploadIdHex, uint64_t& out_offset, uint64_t& out_totalSize);

    /**
     * @brief 获取完整会话以查询状态
     * @param uploadIdHex 16进制的会话ID
     * @param out_session 输出会话模型
     * @return 是否成功找到
     */
    bool get_session(const std::string& uploadIdHex, db::UploadSession& out_session);

    /**
     * @brief 创建一个新的上传会话
     * @param totalSize 文件总大小
     * @param metadataHeader Tus的Upload-Metadata头部内容
     * @param host 客户端发送的Host头部
     * @param out_uploadIdHex 输出生成的16进制会话ID
     * @return 是否成功创建
     */
    bool create_session(uint64_t totalSize, const std::string& metadataHeader, const std::string& host, std::string& out_uploadIdHex);

    /**
     * @brief 追加写入分片数据并推进偏移量
     * @param uploadIdHex 16进制的会话ID
     * @param clientOffset 客户端声明的偏移量
     * @param chunkData 二进制数据块
     * @param out_newOffset 输出写入后最新的偏移量
     * @return 业务结果枚举 UploadChunkResult
     */
    UploadChunkResult write_session_chunk(const std::string& uploadIdHex, uint64_t clientOffset, const std::string& chunkData, uint64_t& out_newOffset);

private:
    void finalize_session(std::string uploadIdHex, std::string realHashHex = "");

    // Atomic helpers for chunk write flow
    UploadChunkResult validate_session_offset(const db::UploadSession& session, uint64_t clientOffset, uint64_t chunkSize);
    bool write_chunk_to_file(const std::string& uploadIdHex, uint64_t offset, const std::string& chunkData);

    // Atomic helpers for finalization flow
    bool compute_file_hash(const std::string& partPath, std::string& out_hashHex);
    bool verify_expected_hash(const std::string& uploadIdBinary, const std::string& realHashHex);
    bool commit_to_object_store(const std::string& partPath, const std::string& realHashHex);
    void mark_session_completed(const std::string& uploadIdBinary, const std::string& realHashHex);
    void mark_session_failed(const std::string& uploadIdBinary, const std::string& errorMsg);

    // Utility helpers
    std::string get_part_file_path(const std::string& uploadIdHex) const;

    bool reconstruct_hasher_from_file(const std::string& partPath, uint64_t limitOffset, blake3_hasher& out_hasher);
    void clean_expired_hashers_under_lock();

private:
    soci::connection_pool& pool_;
    std::string storageRoot_;
    ObjectStore store_;

    std::unordered_map<std::string, ActiveHasher> activeHashers_;
    std::mutex hashersMutex_;
};

} // namespace filelink
