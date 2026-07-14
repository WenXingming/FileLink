#pragma once

#include "db/File.h"
#include "db/Share.h"

#include <ctime>
#include <string>
#include <vector>

namespace soci {
class connection_pool;
}

namespace filelink {

// ========================================================
// CreateShareResult：创建分享链接操作的处理结果。
// ========================================================
enum class CreateShareResult {
    Success,
    FileNotFound,
    InvalidExpiry,
    SystemError
};

// ========================================================
// RevokeShareResult：撤销分享链接操作的处理结果。
// ========================================================
enum class RevokeShareResult {
    Success,
    FileNotFound,
    ShareNotFound,
    SystemError
};

// ==================================================================
// CreatedShare：创建成功后仅返回一次的原始分享令牌及公开元数据。
// ==================================================================
struct CreatedShare {
    std::string share_id;
    std::string token;
    std::tm expires_at{};
};

// ========================================================================
// ShareService：处理文件所有者创建、查看和撤销分享链接的业务规则。
// ========================================================================
class ShareService {
public:
    explicit ShareService(soci::connection_pool& pool) : pool_(pool) {}

    CreateShareResult create_share(const std::string& owner_user_id,
        const std::string& file_id,
        std::time_t expires_at,
        CreatedShare& out_share);
    bool list_shares(const std::string& owner_user_id,
        const std::string& file_id,
        std::vector<db::Share>& out_shares);
    bool find_shared_file(const std::string& token, db::File& out_file);
    RevokeShareResult revoke_share(const std::string& owner_user_id,
        const std::string& file_id,
        const std::string& share_id);

private:
    soci::connection_pool& pool_;
};

} // namespace filelink
