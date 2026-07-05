#pragma once
#include <string>
#include <cstdint>

#include <ctime>

namespace filelink {
namespace models {

struct UploadSession {
    std::string upload_id;       // BINARY(16) mapped to std::string
    std::string state;           // VARCHAR(16)
    std::string file_name;       // VARCHAR(255)
    uint64_t total_size;         // BIGINT UNSIGNED
    uint64_t committed_offset;   // BIGINT UNSIGNED
    bool has_expected_hash = false;
    std::string expected_hash;                 // BINARY(32) mapped to std::string
    bool has_content_hash = false;
    std::string content_hash;                  // BINARY(32) mapped to std::string
    bool has_failure_reason = false;
    std::string failure_reason;                // VARCHAR(255)
    
    std::tm created_at;
    std::tm updated_at;
    std::tm expires_at;
};

} // namespace models
} // namespace filelink
