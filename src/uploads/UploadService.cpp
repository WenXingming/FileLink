#include "uploads/UploadService.h"
#include "database/File.h"
#include "database/Object.h"
#include "database/SociSessionLease.h"
#include "database/UploadSession.h"
#include <soci/soci.h>
#include <soci/connection-pool.h>
#include <utility>
#include <random>
#include <sstream>
#include <iomanip>
#include <vector>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cerrno>
#include "ObjectStore.h"
#include "blake3.h"
#include <thread>
#include <fstream>

namespace filelink {

namespace {

std::string parse_upload_id_to_binary(const std::string& raw) {
    std::string cleaned;
    for (char c : raw) {
        if (c != '-') cleaned.push_back(c);
    }
    if (cleaned.size() == 32) {
        std::string bytes;
        for (std::size_t i = 0; i < 32; i += 2) {
            char high = cleaned[i];
            char low = cleaned[i + 1];
            int h = (high >= 'a') ? (high - 'a' + 10) : ((high >= 'A') ? (high - 'A' + 10) : (high - '0'));
            int l = (low >= 'a') ? (low - 'a' + 10) : ((low >= 'A') ? (low - 'A' + 10) : (low - '0'));
            bytes.push_back(static_cast<char>((h << 4) | l));
        }
        return bytes;
    }
    return raw;
}

std::string base64_decode(const std::string& in) {
    std::string out;
    std::vector<int> T(256, -1);
    const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int i = 0; i < 64; i++) T[chars[i]] = i;

    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (T[c] == -1) continue;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

std::string generate_random_uuid_binary() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<unsigned short> dis(0, 255);
    std::string bytes;
    bytes.reserve(16);
    for (int i = 0; i < 16; ++i) {
        bytes.push_back(static_cast<char>(dis(gen)));
    }
    bytes[6] = (bytes[6] & 0x0F) | 0x40; // Version 4
    bytes[8] = (bytes[8] & 0x3F) | 0x80; // Variant 1
    return bytes;
}

std::string bytes_to_hex(const std::string& bytes) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned char c : bytes) {
        ss << std::setw(2) << static_cast<int>(c);
    }
    return ss.str();
}

std::string hex_to_bytes(const std::string& hex) {
    std::string bytes;
    bytes.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        char high = hex[i];
        char low = hex[i + 1];
        int h = (high >= 'a') ? (high - 'a' + 10) : ((high >= 'A') ? (high - 'A' + 10) : (high - '0'));
        int l = (low >= 'a') ? (low - 'a' + 10) : ((low >= 'A') ? (low - 'A' + 10) : (low - '0'));
        bytes.push_back(static_cast<char>((h << 4) | l));
    }
    return bytes;
}

struct UploadMetadata {
    std::string file_name;
    std::string expected_hash_hex;
};

UploadMetadata parse_upload_metadata(const std::string& header) {
    UploadMetadata metadata;
    std::size_t begin = 0;
    while (begin < header.size()) {
        const std::size_t end = header.find(',', begin);
        const std::string item = header.substr(begin, end - begin);
        const std::size_t separator = item.find(' ');
        if (separator != std::string::npos) {
            const std::string key = item.substr(0, separator);
            const std::string value = base64_decode(item.substr(separator + 1));
            if (key == "filename") {
                metadata.file_name = value;
            } else if (key == "expected_hash") {
                metadata.expected_hash_hex = value;
            }
        }
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return metadata;
}

bool object_matches_upload(const ObjectStore& store, const std::string& hash_hex, uint64_t byte_size) {
    if (hash_hex.size() != 64) {
        return false;
    }

    struct stat info;
    const std::string path = store.get_object_path(hash_hex);
    return ::stat(path.c_str(), &info) == 0
        && S_ISREG(info.st_mode)
        && static_cast<uint64_t>(info.st_size) == byte_size;
}

db::UploadSession make_upload_session(const std::string& upload_id, const std::string& owner_user_id,
    uint64_t total_size, const UploadMetadata& metadata, const std::string& expected_hash,
    bool is_deduplicated) {
    db::UploadSession session;
    session.upload_id = upload_id;
    session.owner_user_id = owner_user_id;
    session.file_name = metadata.file_name.empty() ? "upload_" + bytes_to_hex(upload_id) + ".bin"
        : metadata.file_name;
    session.total_size = total_size;
    session.state = is_deduplicated ? "FINALIZING" : "UPLOADING";
    session.committed_offset = is_deduplicated ? total_size : 0;
    session.expected_hash = expected_hash;
    session.has_expected_hash = !expected_hash.empty();
    session.has_content_hash = false;
    session.has_failure_reason = false;

    const std::time_t expires_at = std::time(nullptr) + 86400;
    localtime_r(&expires_at, &session.expires_at);
    return session;
}

} // namespace

UploadService::UploadService(soci::connection_pool& pool, std::string storageRoot, ObjectStore store)
    : pool_(pool), storageRoot_(std::move(storageRoot)), store_(std::move(store)) {
}

bool UploadService::get_session_progress(const std::string& ownerUserId, const std::string& uploadIdHex,
    uint64_t& out_offset, uint64_t& out_totalSize) {
    std::string uploadIdBinary = parse_upload_id_to_binary(uploadIdHex);
    
    db::SociSessionLease lease(pool_);
    db::UploadSessionDao sessionStore(lease.get());
    db::UploadSession session;
    if (!sessionStore.find(uploadIdBinary, session) || session.owner_user_id != ownerUserId) {
        return false;
    }
    
    out_offset = session.committed_offset;
    out_totalSize = session.total_size;
    return true;
}

bool UploadService::get_session(const std::string& ownerUserId, const std::string& uploadIdHex,
    db::UploadSession& out_session) {
    std::string uploadIdBinary = parse_upload_id_to_binary(uploadIdHex);
    
    db::SociSessionLease lease(pool_);
    db::UploadSessionDao sessionStore(lease.get());
    return sessionStore.find(uploadIdBinary, out_session) && out_session.owner_user_id == ownerUserId;
}

bool UploadService::create_session(const std::string& ownerUserId, uint64_t totalSize,
    const std::string& metadataHeader, const std::string&, std::string& out_uploadIdHex) {
    const UploadMetadata metadata = parse_upload_metadata(metadataHeader);
    const std::string upload_id = generate_random_uuid_binary();
    const std::string expected_hash = metadata.expected_hash_hex.size() == 64
        ? hex_to_bytes(metadata.expected_hash_hex) : "";
    const bool is_deduplicated = object_matches_upload(store_, metadata.expected_hash_hex, totalSize);

    out_uploadIdHex = bytes_to_hex(upload_id);
    const db::UploadSession session = make_upload_session(upload_id, ownerUserId, totalSize,
        metadata, expected_hash, is_deduplicated);

    {
        db::SociSessionLease lease(pool_);
        db::UploadSessionDao(lease.get()).create(session);
    }

    if (is_deduplicated && !complete_published_session(upload_id, metadata.expected_hash_hex)) {
        mark_session_failed(upload_id, "Failed to create logical file");
        return false;
    }
    return true;
}

UploadChunkResult UploadService::write_session_chunk(const std::string& ownerUserId,
    const std::string& uploadIdHex, uint64_t clientOffset, const std::string& chunkData,
    uint64_t& out_newOffset) {
    std::string uploadIdBinary = parse_upload_id_to_binary(uploadIdHex);

    try {
        db::SociSessionLease lease(pool_);
        soci::session& sql = lease.get();
        soci::transaction tr(sql);

        db::UploadSessionDao sessionStore(sql);
        db::UploadSession session;
        if (!sessionStore.find(uploadIdBinary, session) || session.owner_user_id != ownerUserId) {
            return UploadChunkResult::SessionNotFound;
        }

        UploadChunkResult validation = validate_session_offset(session, clientOffset, chunkData.size());
        if (validation != UploadChunkResult::Success) {
            return validation;
        }

        if (!write_chunk_to_file(uploadIdHex, clientOffset, chunkData)) {
            return UploadChunkResult::SystemError;
        }

        const uint64_t newOffset = clientOffset + chunkData.size();
        const bool isComplete = newOffset == session.total_size;
        const std::string realHashHex = update_stream_hash(uploadIdHex,
            get_part_file_path(uploadIdHex), clientOffset, chunkData, isComplete);

        sessionStore.update_offset(uploadIdBinary, newOffset);
        if (isComplete) {
            sessionStore.update_state(uploadIdBinary, "FINALIZING");
        }

        tr.commit();
        out_newOffset = newOffset;

        if (isComplete) {
            std::thread([this, uploadIdHex, realHashHex]() {
                this->finalize_session(uploadIdHex, realHashHex);
            }).detach();
        }

        return UploadChunkResult::Success;
    } catch (...) {
        {
            std::lock_guard<std::mutex> lock(hashersMutex_);
            activeHashers_.erase(uploadIdHex);
        }
        return UploadChunkResult::SystemError;
    }
}

UploadTerminationResult UploadService::terminate_session(const std::string& ownerUserId,
    const std::string& uploadIdHex) {
    std::string uploadIdBinary = parse_upload_id_to_binary(uploadIdHex);
    
    try {
        db::SociSessionLease lease(pool_);
        soci::session& sql = lease.get();
        
        db::UploadSessionDao sessionStore(sql);
        db::UploadSession session;
        if (!sessionStore.find(uploadIdBinary, session) || session.owner_user_id != ownerUserId) {
            return UploadTerminationResult::SessionNotFound;
        }

        if (session.state == "FINALIZING") {
            return UploadTerminationResult::Finalizing;
        }
        if (session.state != "UPLOADING") {
            return UploadTerminationResult::SessionNotFound;
        }

        // 1. Erase from active hashers map
        {
            std::lock_guard<std::mutex> lock(hashersMutex_);
            activeHashers_.erase(uploadIdHex);
        }

        // 2. Physically remove temporary file
        std::string partPath = get_part_file_path(uploadIdHex);
        struct stat st;
        if (::stat(partPath.c_str(), &st) == 0) {
            ::unlink(partPath.c_str());
        }

        // 3. Update state in db to ABORTED
        soci::transaction tr(sql);
        sessionStore.update_state(uploadIdBinary, "ABORTED");
        tr.commit();

        return UploadTerminationResult::Terminated;
    } catch (...) {
        return UploadTerminationResult::SystemError;
    }
}

void UploadService::finalize_session(std::string uploadIdHex, std::string realHashHex) {
    std::string uploadIdBinary = parse_upload_id_to_binary(uploadIdHex);
    std::string partPath = get_part_file_path(uploadIdHex);

    // 1. Compute BLAKE3 if not precomputed (Fallback)
    if (realHashHex.empty()) {
        if (!compute_file_hash(partPath, realHashHex)) {
            mark_session_failed(uploadIdBinary, "Hashing failed");
            return;
        }
    }

    // 2. Compare with expected_hash (if set)
    if (!verify_expected_hash(uploadIdBinary, realHashHex)) {
        ::unlink(partPath.c_str());
        return;
    }

    // 3. Commit to ObjectStore
    if (!commit_to_object_store(partPath, realHashHex)) {
        mark_session_failed(uploadIdBinary, "Failed to commit to store");
        return;
    }

    // 4. Create the logical file and complete the session in one transaction.
    if (!complete_published_session(uploadIdBinary, realHashHex)) {
        mark_session_failed(uploadIdBinary, "Failed to create logical file");
    }
}

// Atomic helpers for chunk write flow
UploadChunkResult UploadService::validate_session_offset(const db::UploadSession& session, uint64_t clientOffset, uint64_t chunkSize) {
    if (session.committed_offset != clientOffset) {
        return UploadChunkResult::OffsetMismatch;
    }
    if (clientOffset + chunkSize > session.total_size) {
        return UploadChunkResult::InvalidChunkSize;
    }
    return UploadChunkResult::Success;
}

bool UploadService::write_chunk_to_file(const std::string& uploadIdHex, uint64_t offset, const std::string& chunkData) {
    struct stat rootSt;
    if (::stat(storageRoot_.c_str(), &rootSt) != 0) {
        if (::mkdir(storageRoot_.c_str(), 0755) != 0 && errno != EEXIST) {
            return false;
        }
    }

    std::string uploadsDir = storageRoot_ + "/uploads";
    struct stat st;
    if (::stat(uploadsDir.c_str(), &st) != 0) {
        if (::mkdir(uploadsDir.c_str(), 0755) != 0 && errno != EEXIST) {
            return false;
        }
    }

    std::string partPath = get_part_file_path(uploadIdHex);
    int flags = O_WRONLY | O_CREAT;
    if (offset == 0) {
        flags |= O_TRUNC;
    }

    int fd = ::open(partPath.c_str(), flags, 0644);
    if (fd < 0) {
        return false;
    }

    if (offset > 0) {
        if (::lseek(fd, offset, SEEK_SET) == -1) {
            ::close(fd);
            return false;
        }
    }

    if (!chunkData.empty()) {
        ssize_t written = ::write(fd, chunkData.data(), chunkData.size());
        if (written != static_cast<ssize_t>(chunkData.size())) {
            ::close(fd);
            return false;
        }
    }

    if (::fdatasync(fd) != 0) {
        ::close(fd);
        return false;
    }

    ::close(fd);
    return true;
}

std::string UploadService::update_stream_hash(const std::string& uploadIdHex,
    const std::string& partPath, uint64_t offset, const std::string& chunkData, bool is_complete) {
    std::unique_lock<std::mutex> lock(hashersMutex_);
    if (activeHashers_.size() > 128) {
        clean_expired_hashers_under_lock();
    }

    auto hasher = activeHashers_.find(uploadIdHex);
    if (hasher == activeHashers_.end() || hasher->second.current_offset != offset) {
        lock.unlock();
        blake3_hasher restored;
        bool restored_ok = true;
        if (offset == 0) {
            blake3_hasher_init(&restored);
        } else {
            restored_ok = reconstruct_hasher_from_file(partPath, offset, restored);
        }
        lock.lock();

        if (!restored_ok) {
            activeHashers_.erase(uploadIdHex);
            return "";
        }
        activeHashers_[uploadIdHex] = {restored, offset, std::chrono::steady_clock::now()};
        hasher = activeHashers_.find(uploadIdHex);
    }

    blake3_hasher_update(&hasher->second.hasher, chunkData.data(), chunkData.size());
    hasher->second.current_offset += chunkData.size();
    hasher->second.last_active = std::chrono::steady_clock::now();
    if (!is_complete) {
        return "";
    }

    uint8_t output[BLAKE3_OUT_LEN];
    blake3_hasher_finalize(&hasher->second.hasher, output, BLAKE3_OUT_LEN);
    activeHashers_.erase(hasher);

    std::stringstream stream;
    stream << std::hex << std::setfill('0');
    for (int index = 0; index < BLAKE3_OUT_LEN; ++index) {
        stream << std::setw(2) << static_cast<int>(output[index]);
    }
    return stream.str();
}

// Atomic helpers for finalization flow
bool UploadService::compute_file_hash(const std::string& partPath, std::string& out_hashHex) {
    try {
        std::ifstream ifs(partPath, std::ios::binary);
        if (!ifs) {
            return false;
        }

        blake3_hasher hasher;
        blake3_hasher_init(&hasher);

        char buffer[65536];
        while (ifs.read(buffer, sizeof(buffer))) {
            blake3_hasher_update(&hasher, buffer, ifs.gcount());
        }
        if (ifs.gcount() > 0) {
            blake3_hasher_update(&hasher, buffer, ifs.gcount());
        }

        uint8_t hashOutput[BLAKE3_OUT_LEN];
        blake3_hasher_finalize(&hasher, hashOutput, BLAKE3_OUT_LEN);

        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (int i = 0; i < BLAKE3_OUT_LEN; ++i) {
            ss << std::setw(2) << static_cast<int>(hashOutput[i]);
        }
        out_hashHex = ss.str();
        return true;
    } catch (...) {
        return false;
    }
}

bool UploadService::verify_expected_hash(const std::string& uploadIdBinary, const std::string& realHashHex) {
    try {
        db::SociSessionLease lease(pool_);
        db::UploadSessionDao sessionStore(lease.get());
        db::UploadSession session;
        if (!sessionStore.find(uploadIdBinary, session)) {
            return false;
        }

        if (session.has_expected_hash) {
            std::string realHashBytes = hex_to_bytes(realHashHex);
            if (session.expected_hash != realHashBytes) {
                sessionStore.update_failed(uploadIdBinary, "BLAKE3 checksum mismatch");
                return false;
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool UploadService::commit_to_object_store(const std::string& partPath, const std::string& realHashHex) {
    try {
        CommitResult result = store_.commit(partPath, realHashHex);
        return (result.status == CommitStatus::Created || result.status == CommitStatus::Reused);
    } catch (...) {
        return false;
    }
}

bool UploadService::complete_published_session(const std::string& uploadIdBinary,
    const std::string& realHashHex) {
    try {
        db::SociSessionLease lease(pool_);
        soci::session& sql = lease.get();
        soci::transaction transaction(sql);

        db::UploadSessionDao sessionStore(sql);
        db::UploadSession session;
        if (!sessionStore.find(uploadIdBinary, session) || session.state != "FINALIZING") {
            return false;
        }

        const std::string hashBytes = hex_to_bytes(realHashHex);
        if (db::ObjectDao(sql).add_reference(hashBytes, session.total_size)
            == db::ObjectReferenceResult::Reclaiming) {
            return false;
        }

        db::File file;
        file.file_id = generate_random_uuid_binary();
        file.owner_user_id = session.owner_user_id;
        file.content_hash = hashBytes;
        file.display_name = session.file_name;
        db::FileDao(sql).create(file);

        sessionStore.update_completed(uploadIdBinary, hashBytes);
        sessionStore.set_completed_file(uploadIdBinary, file.file_id);
        transaction.commit();
        return true;
    } catch (...) {
        return false;
    }
}

void UploadService::mark_session_failed(const std::string& uploadIdBinary, const std::string& errorMsg) {
    try {
        db::SociSessionLease lease(pool_);
        db::UploadSessionDao sessionStore(lease.get());
        sessionStore.update_failed(uploadIdBinary, errorMsg);
    } catch (...) {}
}

// Utility helpers
std::string UploadService::get_part_file_path(const std::string& uploadIdHex) const {
    return storageRoot_ + "/uploads/" + uploadIdHex + ".part";
}

bool UploadService::reconstruct_hasher_from_file(const std::string& partPath, uint64_t limitOffset, blake3_hasher& out_hasher) {
    std::ifstream ifs(partPath, std::ios::binary);
    if (!ifs) {
        return false;
    }

    blake3_hasher_init(&out_hasher);
    char buffer[65536];
    uint64_t readBytes = 0;

    while (readBytes < limitOffset) {
        uint64_t toRead = std::min(static_cast<uint64_t>(sizeof(buffer)), limitOffset - readBytes);
        ifs.read(buffer, toRead);
        std::streamsize bytesRead = ifs.gcount();
        if (bytesRead <= 0) {
            break;
        }
        blake3_hasher_update(&out_hasher, buffer, bytesRead);
        readBytes += bytesRead;
    }
    return (readBytes == limitOffset);
}

void UploadService::clean_expired_hashers_under_lock() {
    auto now = std::chrono::steady_clock::now();
    for (auto it = activeHashers_.begin(); it != activeHashers_.end(); ) {
        if (now - it->second.last_active > std::chrono::hours(1)) {
            it = activeHashers_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace filelink
