#include "UploadService.h"
#include "store/UploadSessionStore.h"
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

} // namespace

UploadService::UploadService(soci::connection_pool& pool, std::string storageRoot, ObjectStore store)
    : pool_(pool), storageRoot_(std::move(storageRoot)), store_(std::move(store)) {
}

bool UploadService::get_session_progress(const std::string& uploadIdHex, uint64_t& out_offset, uint64_t& out_totalSize) {
    std::string uploadIdBinary = parse_upload_id_to_binary(uploadIdHex);
    
    SociSessionLease lease(pool_);
    store::UploadSessionStore sessionStore(lease.get());
    models::UploadSession session;
    if (!sessionStore.find(uploadIdBinary, session)) {
        return false;
    }
    
    out_offset = session.committed_offset;
    out_totalSize = session.total_size;
    return true;
}

bool UploadService::get_session(const std::string& uploadIdHex, models::UploadSession& out_session) {
    std::string uploadIdBinary = parse_upload_id_to_binary(uploadIdHex);
    
    SociSessionLease lease(pool_);
    store::UploadSessionStore sessionStore(lease.get());
    return sessionStore.find(uploadIdBinary, out_session);
}

bool UploadService::create_session(uint64_t totalSize, const std::string& metadataHeader, const std::string&, std::string& out_uploadIdHex) {
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

    SociSessionLease lease(pool_);
    store::UploadSessionStore sessionStore(lease.get());

    models::UploadSession session;
    session.upload_id = uploadIdBinary;
    session.state = "UPLOADING";
    session.file_name = filename.empty() ? ("upload_" + out_uploadIdHex + ".bin") : filename;
    session.total_size = totalSize;
    session.committed_offset = 0;

    if (expectedHash.size() == 64) {
        std::string hashBytes;
        for (std::size_t i = 0; i < 64; i += 2) {
            char high = expectedHash[i];
            char low = expectedHash[i + 1];
            int h = (high >= 'a') ? (high - 'a' + 10) : ((high >= 'A') ? (high - 'A' + 10) : (high - '0'));
            int l = (low >= 'a') ? (low - 'a' + 10) : ((low >= 'A') ? (low - 'A' + 10) : (low - '0'));
            hashBytes.push_back(static_cast<char>((h << 4) | l));
        }
        session.expected_hash = hashBytes;
        session.has_expected_hash = true;
    } else {
        session.has_expected_hash = false;
    }

    session.has_content_hash = false;
    session.has_failure_reason = false;

    std::time_t t = std::time(nullptr) + 86400; // 24 hours
    session.expires_at = *std::localtime(&t);

    sessionStore.create(session);
    return true;
}

int UploadService::write_session_chunk(const std::string& uploadIdHex, uint64_t clientOffset, const std::string& chunkData, uint64_t& out_newOffset) {
    std::string uploadIdBinary = parse_upload_id_to_binary(uploadIdHex);

    try {
        SociSessionLease lease(pool_);
        soci::session& sql = lease.get();
        soci::transaction tr(sql);

        store::UploadSessionStore sessionStore(sql);
        models::UploadSession session;
        if (!sessionStore.find(uploadIdBinary, session)) {
            return 404;
        }

        if (session.committed_offset != clientOffset) {
            return 409;
        }

        if (clientOffset + chunkData.size() > session.total_size) {
            return 400;
        }

        struct stat rootSt;
        if (::stat(storageRoot_.c_str(), &rootSt) != 0) {
            if (::mkdir(storageRoot_.c_str(), 0755) != 0 && errno != EEXIST) {
                return 500;
            }
        }

        std::string uploadsDir = storageRoot_ + "/uploads";
        struct stat st;
        if (::stat(uploadsDir.c_str(), &st) != 0) {
            if (::mkdir(uploadsDir.c_str(), 0755) != 0 && errno != EEXIST) {
                return 500;
            }
        }

        std::string partPath = uploadsDir + "/" + uploadIdHex + ".part";
        
        int flags = O_WRONLY | O_CREAT;
        if (clientOffset == 0) {
            flags |= O_TRUNC;
        }
        
        int fd = ::open(partPath.c_str(), flags, 0644);
        if (fd < 0) {
            return 500;
        }

        if (clientOffset > 0) {
            if (::lseek(fd, clientOffset, SEEK_SET) == -1) {
                ::close(fd);
                return 500;
            }
        }

        if (!chunkData.empty()) {
            ssize_t written = ::write(fd, chunkData.data(), chunkData.size());
            if (written != static_cast<ssize_t>(chunkData.size())) {
                ::close(fd);
                return 500;
            }
        }

        if (::fdatasync(fd) != 0) {
            ::close(fd);
            return 500;
        }

        ::close(fd);

        sessionStore.update_offset(uploadIdBinary, clientOffset + chunkData.size());
        
        bool isComplete = (clientOffset + chunkData.size() == session.total_size);
        if (isComplete) {
            sessionStore.update_state(uploadIdBinary, "FINALIZING");
        }

        tr.commit();

        out_newOffset = clientOffset + chunkData.size();

        if (isComplete) {
            std::thread([this, uploadIdHex]() {
                this->finalize_session(uploadIdHex);
            }).detach();
        }

        return 200;
    } catch (...) {
        return 500;
    }
}

void UploadService::finalize_session(std::string uploadIdHex) {
    std::string uploadIdBinary = parse_upload_id_to_binary(uploadIdHex);
    std::string partPath = storageRoot_ + "/uploads/" + uploadIdHex + ".part";
    
    // 1. Compute BLAKE3
    std::string realHashHex;
    try {
        std::ifstream ifs(partPath, std::ios::binary);
        if (!ifs) {
            throw std::runtime_error("Failed to open part file for hashing");
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
        realHashHex = ss.str();
    } catch (const std::exception& e) {
        try {
            SociSessionLease lease(pool_);
            store::UploadSessionStore sessionStore(lease.get());
            sessionStore.update_failed(uploadIdBinary, std::string("Hashing failed: ") + e.what());
        } catch (...) {}
        return;
    }

    // 2. Compare with expected_hash (if set)
    try {
        SociSessionLease lease(pool_);
        soci::session& sql = lease.get();
        store::UploadSessionStore sessionStore(sql);
        models::UploadSession session;
        if (!sessionStore.find(uploadIdBinary, session)) {
            return;
        }
        
        if (session.has_expected_hash) {
            std::string realHashBytes;
            for (std::size_t i = 0; i < 64; i += 2) {
                char high = realHashHex[i];
                char low = realHashHex[i + 1];
                int h = (high >= 'a') ? (high - 'a' + 10) : ((high >= 'A') ? (high - 'A' + 10) : (high - '0'));
                int l = (low >= 'a') ? (low - 'a' + 10) : ((low >= 'A') ? (low - 'A' + 10) : (low - '0'));
                realHashBytes.push_back(static_cast<char>((h << 4) | l));
            }
            
            if (session.expected_hash != realHashBytes) {
                sessionStore.update_failed(uploadIdBinary, "BLAKE3 checksum mismatch");
                ::unlink(partPath.c_str());
                return;
            }
        }
        
        // 3. Commit to ObjectStore
        CommitResult result = store_.commit(partPath, realHashHex);
        if (result.status != CommitStatus::Created && result.status != CommitStatus::Reused) {
            sessionStore.update_failed(uploadIdBinary, "Failed to commit to store");
            return;
        }
        
        // 4. Update completed state in db
        std::string hashBytes;
        for (std::size_t i = 0; i < 64; i += 2) {
            char high = realHashHex[i];
            char low = realHashHex[i + 1];
            int h = (high >= 'a') ? (high - 'a' + 10) : ((high >= 'A') ? (high - 'A' + 10) : (high - '0'));
            int l = (low >= 'a') ? (low - 'a' + 10) : ((low >= 'A') ? (low - 'A' + 10) : (low - '0'));
            hashBytes.push_back(static_cast<char>((h << 4) | l));
        }
        sessionStore.update_completed(uploadIdBinary, hashBytes);
    } catch (const std::exception& e) {
        try {
            SociSessionLease lease(pool_);
            store::UploadSessionStore sessionStore(lease.get());
            sessionStore.update_failed(uploadIdBinary, std::string("Commit failed: ") + e.what());
        } catch (...) {}
    }
}

} // namespace filelink
