#pragma once
#include "models/UploadSession.h"
#include <soci/soci.h>

#include <string>

namespace filelink {
namespace store {

class UploadSessionStore {
public:
    explicit UploadSessionStore(soci::session& sql) : sql_(sql) {}
    
    void create(const models::UploadSession& session);
    bool find(const std::string& upload_id, models::UploadSession& out_session);
    void update_offset(const std::string& upload_id, uint64_t new_offset);
    void update_state(const std::string& upload_id, const std::string& state);
    void update_completed(const std::string& upload_id, const std::string& content_hash);
    void update_failed(const std::string& upload_id, const std::string& failure_reason);

private:
    soci::session& sql_;
};

} // namespace store
} // namespace filelink
