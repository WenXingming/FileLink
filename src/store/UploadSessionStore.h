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

private:
    soci::session& sql_;
};

} // namespace store
} // namespace filelink
