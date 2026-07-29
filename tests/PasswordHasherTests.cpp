#include "auth/PasswordHasher.h"

#include <gtest/gtest.h>

namespace {

TEST(PasswordHasherTest, HashesAndVerifiesPassword) {
    const std::string encoded_hash = filelink::PasswordHasher::hash("correct horse battery staple");

    EXPECT_NE(encoded_hash, "correct horse battery staple");
    EXPECT_TRUE(filelink::PasswordHasher::verify("correct horse battery staple", encoded_hash));
}

TEST(PasswordHasherTest, RejectsWrongPasswordAndMalformedHash) {
    const std::string encoded_hash = filelink::PasswordHasher::hash("correct password");

    EXPECT_FALSE(filelink::PasswordHasher::verify("wrong password", encoded_hash));
    EXPECT_FALSE(filelink::PasswordHasher::verify("correct password", "not-a-password-hash"));
}

} // namespace
