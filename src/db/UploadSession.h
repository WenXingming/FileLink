#pragma once

#include <soci/soci.h>
#include <string>
#include <cstdint>
#include <ctime>

namespace filelink {
namespace db {

struct UploadSession {
    std::string upload_id;       // BINARY(16) mapped to std::string
    std::string owner_user_id;   // BINARY(16) mapped to std::string
    std::string state;           // VARCHAR(16)
    std::string file_name;       // VARCHAR(255)
    uint64_t total_size = 0;     // BIGINT UNSIGNED
    uint64_t committed_offset = 0; // BIGINT UNSIGNED
    bool has_expected_hash = false;
    std::string expected_hash;   // BINARY(32) mapped to std::string
    bool has_content_hash = false;
    std::string content_hash;    // BINARY(32) mapped to std::string
    bool has_completed_file_id = false;
    std::string completed_file_id; // BINARY(16) mapped to std::string
    bool has_failure_reason = false;
    std::string failure_reason;  // VARCHAR(255)
    
    std::tm created_at{};
    std::tm updated_at{};
    std::tm expires_at{};
};

class UploadSessionDao {
public:
    explicit UploadSessionDao(soci::session& sql) : sql_(sql) {}
    
    void create(const UploadSession& session);
    bool find(const std::string& upload_id, UploadSession& out_session);
    void update_offset(const std::string& upload_id, uint64_t new_offset);
    void update_state(const std::string& upload_id, const std::string& state);
    void update_completed(const std::string& upload_id, const std::string& content_hash);
    void set_completed_file(const std::string& upload_id, const std::string& completed_file_id);
    void update_failed(const std::string& upload_id, const std::string& failure_reason);

private:
    soci::session& sql_;
};

} // namespace db
} // namespace filelink
