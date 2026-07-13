#include "MySqlTestConfig.h"
#include "db/Object.h"

#include <gtest/gtest.h>

#include <soci/mysql/soci-mysql.h>

class ObjectDaoTest : public testing::Test {
protected:
    void SetUp() override {
        sql_.open(soci::mysql, filelink::test::mysql_connection_string());
        clear_objects();
    }

    void TearDown() override {
        if (sql_.is_connected()) {
            clear_objects();
        }
    }

    void clear_objects() {
        sql_ << "DELETE FROM upload_sessions";
        sql_ << "DELETE FROM files";
        sql_ << "DELETE FROM objects";
    }

    soci::session sql_;
};

TEST_F(ObjectDaoTest, CreatesAndFindsObject) {
    filelink::db::ObjectDao objects(sql_);
    filelink::db::Object object;
    object.content_hash = "12345678901234567890123456789012";
    object.byte_size = 42;
    object.ref_count = 3;

    objects.create(object);

    filelink::db::Object found;
    ASSERT_TRUE(objects.find(object.content_hash, found));
    EXPECT_EQ(found.content_hash, object.content_hash);
    EXPECT_EQ(found.byte_size, 42u);
    EXPECT_EQ(found.ref_count, 3u);
    EXPECT_EQ(found.state, "READY");
    EXPECT_FALSE(objects.find("abcdefghijklmnopqrstuvwxzy123456", found));
}
