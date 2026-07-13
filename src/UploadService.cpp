#include "UploadService.h"
#include "db/UploadSession.h"
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

class SociSessionLease {
public:
    SociSessionLease(soci::connection_pool& pool) : pool_(pool), pos_(pool.lease()) {}
    ~SociSessionLease() { pool_.give_back(pos_); }
    soci::session& get() { return pool_.at(pos_); }
private:
    soci::connection_pool& pool_;
    std::size_t pos_;
};

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

} // namespace

UploadService::UploadService(soci::connection_pool& pool, std::string storageRoot, ObjectStore store)
    : pool_(pool), storageRoot_(std::move(storageRoot)), store_(std::move(store)) {
}

bool UploadService::get_session_progress(const std::string& ownerUserId, const std::string& uploadIdHex,
    uint64_t& out_offset, uint64_t& out_totalSize) {
    std::string uploadIdBinary = parse_upload_id_to_binary(uploadIdHex);
    
    SociSessionLease lease(pool_);
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
    
    SociSessionLease lease(pool_);
    db::UploadSessionDao sessionStore(lease.get());
    return sessionStore.find(uploadIdBinary, out_session) && out_session.owner_user_id == ownerUserId;
}

bool UploadService::create_session(const std::string& ownerUserId, uint64_t totalSize,
    const std::string& metadataHeader, const std::string&, std::string& out_uploadIdHex) {
    std::string filename;
    std::string expectedHash;
    
    std::size_t start = 0;
    while (start < metadataHeader.size()) {
        std::size_t comma = metadataHeader.find(',', start);
        std::string pair = metadataHeader.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        std::size_t space = pair.find(' ');
        if (space != std::string::npos) {
            std::string key = pair.substr(0, space);
            std::string encodedVal = pair.substr(space + 1);
            std::string val = base64_decode(encodedVal);
            if (key == "filename") {
                filename = val;
            } else if (key == "expected_hash") {
                expectedHash = val;
            }
        }
        if (comma == std::string::npos) break;
        start = comma + 1;
    }

    std::string uploadIdBinary = generate_random_uuid_binary();
    out_uploadIdHex = bytes_to_hex(uploadIdBinary);

    bool hitDeduplication = false;
    std::string hashBytes;
    if (expectedHash.size() == 64) {
        for (std::size_t i = 0; i < 64; i += 2) {
            char high = expectedHash[i];
            char low = expectedHash[i + 1];
            int h = (high >= 'a') ? (high - 'a' + 10) : ((high >= 'A') ? (high - 'A' + 10) : (high - '0'));
            int l = (low >= 'a') ? (low - 'a' + 10) : ((low >= 'A') ? (low - 'A' + 10) : (low - '0'));
            hashBytes.push_back(static_cast<char>((h << 4) | l));
        }

        std::string objectPath = store_.get_object_path(expectedHash);
        struct stat st;
        if (::stat(objectPath.c_str(), &st) == 0 && S_ISREG(st.st_mode) && static_cast<uint64_t>(st.st_size) == totalSize) {
            hitDeduplication = true;
        }
    }

    SociSessionLease lease(pool_);
    db::UploadSessionDao sessionStore(lease.get());

    db::UploadSession session;
    session.upload_id = uploadIdBinary;
    session.owner_user_id = ownerUserId;
    session.file_name = filename.empty() ? ("upload_" + out_uploadIdHex + ".bin") : filename;
    session.total_size = totalSize;

    if (hitDeduplication) {
        session.state = "COMPLETED";
        session.committed_offset = totalSize;
        session.expected_hash = hashBytes;
        session.has_expected_hash = true;
        session.content_hash = hashBytes;
        session.has_content_hash = true;
    } else {
        session.state = "UPLOADING";
        session.committed_offset = 0;
        if (!hashBytes.empty()) {
            session.expected_hash = hashBytes;
            session.has_expected_hash = true;
        } else {
            session.has_expected_hash = false;
        }
        session.has_content_hash = false;
    }

    session.has_failure_reason = false;

    std::time_t t = std::time(nullptr) + 86400; // 24 hours
    session.expires_at = *std::localtime(&t);

    sessionStore.create(session);
    return true;
}

UploadChunkResult UploadService::write_session_chunk(const std::string& ownerUserId,
    const std::string& uploadIdHex, uint64_t clientOffset, const std::string& chunkData,
    uint64_t& out_newOffset) {
    std::string uploadIdBinary = parse_upload_id_to_binary(uploadIdHex);

    try {
        SociSessionLease lease(pool_);
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

        uint64_t newOffset = clientOffset + chunkData.size();
        bool isComplete = (newOffset == session.total_size);
        std::string realHashHex = "";

        // Stream hashing / Lazy reconstruction
        std::string partPath = get_part_file_path(uploadIdHex);
        {
            std::unique_lock<std::mutex> lock(hashersMutex_);
            if (activeHashers_.size() > 128) {
                clean_expired_hashers_under_lock();
            }

            auto it = activeHashers_.find(uploadIdHex);
            if (it == activeHashers_.end() || it->second.current_offset != clientOffset) {
                lock.unlock(); // Release lock during reconstruction I/O
                blake3_hasher restoredHasher;
                bool ok = true;
                if (clientOffset > 0) {
                    ok = reconstruct_hasher_from_file(partPath, clientOffset, restoredHasher);
                } else {
                    blake3_hasher_init(&restoredHasher);
                }
                
                lock.lock(); // Re-acquire lock
                if (ok) {
                    ActiveHasher ah{restoredHasher, clientOffset, std::chrono::steady_clock::now()};
                    activeHashers_[uploadIdHex] = ah;
                    it = activeHashers_.find(uploadIdHex);
                } else {
                    activeHashers_.erase(uploadIdHex);
                    it = activeHashers_.end();
                }
            }

            if (it != activeHashers_.end()) {
                blake3_hasher_update(&it->second.hasher, chunkData.data(), chunkData.size());
                it->second.current_offset += chunkData.size();
                it->second.last_active = std::chrono::steady_clock::now();

                if (isComplete) {
                    uint8_t hashOutput[BLAKE3_OUT_LEN];
                    blake3_hasher_finalize(&it->second.hasher, hashOutput, BLAKE3_OUT_LEN);

                    std::stringstream ss;
                    ss << std::hex << std::setfill('0');
                    for (int i = 0; i < BLAKE3_OUT_LEN; ++i) {
                        ss << std::setw(2) << static_cast<int>(hashOutput[i]);
                    }
                    realHashHex = ss.str();
                    activeHashers_.erase(it);
                }
            }
        }

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

bool UploadService::terminate_session(const std::string& ownerUserId, const std::string& uploadIdHex) {
    std::string uploadIdBinary = parse_upload_id_to_binary(uploadIdHex);
    
    try {
        SociSessionLease lease(pool_);
        soci::session& sql = lease.get();
        
        db::UploadSessionDao sessionStore(sql);
        db::UploadSession session;
        if (!sessionStore.find(uploadIdBinary, session) || session.owner_user_id != ownerUserId) {
            return false;
        }

        if (session.state != "UPLOADING" && session.state != "FINALIZING") {
            return false;
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

        return true;
    } catch (...) {
        return false;
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

    // 4. Update completed state in db
    mark_session_completed(uploadIdBinary, realHashHex);
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
        SociSessionLease lease(pool_);
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

void UploadService::mark_session_completed(const std::string& uploadIdBinary, const std::string& realHashHex) {
    try {
        SociSessionLease lease(pool_);
        db::UploadSessionDao sessionStore(lease.get());
        std::string hashBytes = hex_to_bytes(realHashHex);
        sessionStore.update_completed(uploadIdBinary, hashBytes);
    } catch (...) {}
}

void UploadService::mark_session_failed(const std::string& uploadIdBinary, const std::string& errorMsg) {
    try {
        SociSessionLease lease(pool_);
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
