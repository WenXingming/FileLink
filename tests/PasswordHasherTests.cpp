#include "auth/PasswordHasher.h"

#include <gtest/gtest.h>

namespace {

TEST(PasswordHasherTest, HashesAndVerifiesPassword) {
    const std::string encodedHash = filelink::PasswordHasher::hash("correct horse battery staple");

    EXPECT_NE(encodedHash, "correct horse battery staple");
    EXPECT_TRUE(filelink::PasswordHasher::verify("correct horse battery staple", encodedHash));
}

TEST(PasswordHasherTest, RejectsWrongPasswordAndMalformedHash) {
    const std::string encodedHash = filelink::PasswordHasher::hash("correct password");

    EXPECT_FALSE(filelink::PasswordHasher::verify("wrong password", encodedHash));
    EXPECT_FALSE(filelink::PasswordHasher::verify("correct password", "not-a-password-hash"));
}

} // namespace
