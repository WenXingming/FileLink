// ============================================================================
// 密码哈希实现：封装 libsodium 初始化及 Argon2id 哈希调用。
// 向 AuthService 隐藏算法参数和底层 C API 细节。
// ============================================================================

#include "PasswordHasher.h"

#include <sodium.h>

#include <array>
#include <stdexcept>

namespace filelink {

namespace {

void ensure_sodium_ready() {
    if (sodium_init() < 0) {
        throw std::runtime_error("libsodium initialization failed");
    }
}

} // namespace

std::string PasswordHasher::hash(const std::string& password) {
    ensure_sodium_ready();

    std::array<char, crypto_pwhash_STRBYTES> encoded_hash{};
    if (crypto_pwhash_str_alg(
            encoded_hash.data(),
            password.data(),
            static_cast<unsigned long long>(password.size()),
            crypto_pwhash_OPSLIMIT_INTERACTIVE,
            crypto_pwhash_MEMLIMIT_INTERACTIVE,
            crypto_pwhash_ALG_ARGON2ID13) != 0) {
        throw std::runtime_error("password hashing failed");
    }

    return encoded_hash.data();
}

bool PasswordHasher::verify(const std::string& password, const std::string& encoded_hash) {
    ensure_sodium_ready();
    return crypto_pwhash_str_verify(
        encoded_hash.c_str(),
        password.data(),
        static_cast<unsigned long long>(password.size())) == 0;
}

} // namespace filelink
