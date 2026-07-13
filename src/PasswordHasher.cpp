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

    std::array<char, crypto_pwhash_STRBYTES> encodedHash{};
    if (crypto_pwhash_str_alg(
            encodedHash.data(),
            password.data(),
            static_cast<unsigned long long>(password.size()),
            crypto_pwhash_OPSLIMIT_INTERACTIVE,
            crypto_pwhash_MEMLIMIT_INTERACTIVE,
            crypto_pwhash_ALG_ARGON2ID13) != 0) {
        throw std::runtime_error("password hashing failed");
    }

    return encodedHash.data();
}

bool PasswordHasher::verify(const std::string& password, const std::string& encodedHash) {
    ensure_sodium_ready();
    return crypto_pwhash_str_verify(
        encodedHash.c_str(),
        password.data(),
        static_cast<unsigned long long>(password.size())) == 0;
}

} // namespace filelink
