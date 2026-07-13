#pragma once

#include "ObjectStore.h"
#include "blake3.h"
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace soci {
class connection_pool;
}

namespace filelink {

// ==============================================================
// UploadChunkResult：写入一个上传分片后的处理结果。
// ==============================================================
enum class UploadChunkResult {
    Success,
    SessionNotFound,
    OffsetMismatch,
    InvalidChunkSize,
    SystemError
};

// ===========================================================
// UploadTerminationResult：取消上传会话后的处理结果。
// ===========================================================
enum class UploadTerminationResult {
    Terminated,
    SessionNotFound,
    Finalizing,
    SystemError
};

namespace db {
struct UploadSession;
}

// ======================================================================
// UploadService：管理上传会话、分片写入、内容校验与对象发布。
// ======================================================================
class UploadService {
public:
    UploadService(soci::connection_pool& pool, std::string storageRoot, ObjectStore store);
    ~UploadService() = default;

    UploadService(const UploadService&) = delete;
    UploadService& operator=(const UploadService&) = delete;

    // 查询上传会话
    bool get_session_progress(const std::string& ownerUserId, const std::string& uploadIdHex,
        uint64_t& out_offset, uint64_t& out_totalSize);
    bool get_session(const std::string& ownerUserId, const std::string& uploadIdHex,
        db::UploadSession& out_session);

    // 创建与写入上传会话
    bool create_session(const std::string& ownerUserId, uint64_t totalSize,
        const std::string& metadataHeader, const std::string& host, std::string& out_uploadIdHex);
    UploadChunkResult write_session_chunk(const std::string& ownerUserId, const std::string& uploadIdHex,
        uint64_t clientOffset, const std::string& chunkData, uint64_t& out_newOffset);
    UploadTerminationResult terminate_session(const std::string& ownerUserId,
        const std::string& uploadIdHex);

private:
    // 分片写入
    UploadChunkResult validate_session_offset(const db::UploadSession& session,
        uint64_t clientOffset, uint64_t chunkSize);
    bool write_chunk_to_file(const std::string& uploadIdHex, uint64_t offset,
        const std::string& chunkData);
    std::string update_stream_hash(const std::string& uploadIdHex, const std::string& partPath,
        uint64_t offset, const std::string& chunkData, bool is_complete);

    // 完成上传
    void finalize_session(std::string uploadIdHex, std::string realHashHex = "");
    bool compute_file_hash(const std::string& partPath, std::string& out_hashHex);
    bool verify_expected_hash(const std::string& uploadIdBinary, const std::string& realHashHex);
    bool commit_to_object_store(const std::string& partPath, const std::string& realHashHex);
    bool complete_published_session(const std::string& uploadIdBinary, const std::string& realHashHex);
    void mark_session_failed(const std::string& uploadIdBinary, const std::string& errorMsg);

    // 哈希缓存与临时文件
    std::string get_part_file_path(const std::string& uploadIdHex) const;
    bool reconstruct_hasher_from_file(const std::string& partPath, uint64_t limitOffset,
        blake3_hasher& out_hasher);
    void clean_expired_hashers_under_lock();

    // =====================================================================
    // ActiveHasher：保存未完成上传的流式哈希器及其最近写入位置。
    // =====================================================================
    struct ActiveHasher {
        blake3_hasher hasher;
        uint64_t current_offset = 0;
        std::chrono::steady_clock::time_point last_active;
    };

    soci::connection_pool& pool_;
    std::string storageRoot_;
    ObjectStore store_;

    std::unordered_map<std::string, ActiveHasher> activeHashers_;
    std::mutex hashersMutex_;
};

} // namespace filelink
