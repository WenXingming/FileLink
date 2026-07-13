#pragma once

#include <string>

namespace filelink {

// ====================================================================
// PasswordHasher：使用 Argon2id 生成并验证密码哈希，不保存明文密码。
// ====================================================================
class PasswordHasher {
public:
    static std::string hash(const std::string& password);
    static bool verify(const std::string& password, const std::string& encodedHash);
};

} // namespace filelink
