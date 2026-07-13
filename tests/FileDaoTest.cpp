#include "MySqlTestConfig.h"
#include "db/File.h"
#include "db/Object.h"
#include "db/User.h"

#include <gtest/gtest.h>

#include <chrono>
#include <soci/mysql/soci-mysql.h>
#include <thread>

class FileDaoTest : public testing::Test {
protected:
    void SetUp() override {
        sql_.open(soci::mysql, filelink::test::mysql_connection_string());
        clear_database();
    }

    void TearDown() override {
        if (sql_.is_connected()) {
            clear_database();
        }
    }

    void clear_database() {
        sql_ << "DELETE FROM upload_sessions";
        sql_ << "DELETE FROM files";
        sql_ << "DELETE FROM objects";
        sql_ << "DELETE FROM user_sessions";
        sql_ << "DELETE FROM users";
    }

    void create_user(const std::string& user_id, const std::string& username) {
        filelink::db::User user;
        user.user_id = user_id;
        user.username = username;
        user.password_hash = "$argon2id$test";
        filelink::db::UserDao(sql_).create(user);
    }

    void create_object(const std::string& content_hash) {
        filelink::db::ObjectDao(sql_).add_reference(content_hash, 42);
    }

    soci::session sql_;
};

TEST_F(FileDaoTest, CreatesAndListsOnlyOwnersFilesNewestFirst) {
    const std::string alice_id = "1234567890123456";
    const std::string bob_id = "abcdefghijklmnop";
    const std::string content_hash = "12345678901234567890123456789012";
    create_user(alice_id, "alice");
    create_user(bob_id, "bob");
    create_object(content_hash);

    filelink::db::FileDao files(sql_);
    files.create({"first-file-id-01", alice_id, content_hash, "first.txt", {}});
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    files.create({"second-file-id02", alice_id, content_hash, "second.txt", {}});
    files.create({"third-file-id03", bob_id, content_hash, "private.txt", {}});

    std::vector<filelink::db::File> alice_files;
    files.find_by_owner(alice_id, alice_files);
    ASSERT_EQ(alice_files.size(), 2u);
    EXPECT_EQ(alice_files[0].display_name, "second.txt");
    EXPECT_EQ(alice_files[0].owner_user_id, alice_id);
    EXPECT_EQ(alice_files[1].display_name, "first.txt");
    EXPECT_EQ(alice_files[1].content_hash, content_hash);

    filelink::db::File found_file;
    EXPECT_TRUE(files.find_by_id_and_owner("second-file-id02", alice_id, found_file));
    EXPECT_EQ(found_file.display_name, "second.txt");
    EXPECT_FALSE(files.find_by_id_and_owner("second-file-id02", bob_id, found_file));

    std::vector<filelink::db::File> bob_files;
    files.find_by_owner(bob_id, bob_files);
    ASSERT_EQ(bob_files.size(), 1u);
    EXPECT_EQ(bob_files[0].display_name, "private.txt");

    EXPECT_FALSE(files.remove_by_id_and_owner("second-file-id02", bob_id));
    EXPECT_TRUE(files.remove_by_id_and_owner("second-file-id02", alice_id));
    EXPECT_FALSE(files.remove_by_id_and_owner("second-file-id02", alice_id));
    files.find_by_owner(alice_id, alice_files);
    ASSERT_EQ(alice_files.size(), 1u);
    EXPECT_EQ(alice_files[0].display_name, "first.txt");
}
