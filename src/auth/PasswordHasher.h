// ============================================================================
// 密码哈希接口：生成并验证 libsodium Argon2id 格式的密码哈希。
// 不保存明文密码，也不负责用户查询、登录流程或 HTTP 响应。
// ============================================================================

#pragma once

#include <string>

namespace filelink {

class PasswordHasher {
public:
    static std::string hash(const std::string& password);
    static bool verify(const std::string& password, const std::string& encoded_hash);
};

} // namespace filelink
