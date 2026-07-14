#include "MySqlTestConfig.h"
#include "storage/ObjectStore.h"
#include "cleaner/ObjectReclaimer.h"
#include "database/Object.h"

#include <gtest/gtest.h>

#include <fstream>
#include <memory>
#include <soci/connection-pool.h>
#include <soci/mysql/soci-mysql.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

class ObjectReclaimerTest : public testing::Test {
protected:
    void SetUp() override {
        pool_ = std::make_unique<soci::connection_pool>(1);
        pool_->at(0).open(soci::mysql, filelink::test::mysql_connection_string());
        clear_objects();

        storage_root_ = "./storage_test_reclaimer";
        ::mkdir(storage_root_.c_str(), 0755);
    }

    void TearDown() override {
        clear_objects();
        const std::string hash_hex(64, 'a');
        ::unlink((storage_root_ + "/objects/aa/aa/" + hash_hex).c_str());
        ::rmdir((storage_root_ + "/objects/aa/aa").c_str());
        ::rmdir((storage_root_ + "/objects/aa").c_str());
        ::rmdir((storage_root_ + "/objects").c_str());
        ::rmdir(storage_root_.c_str());
    }

    void clear_objects() {
        if (pool_ != nullptr) {
            soci::session sql(*pool_);
            sql << "DELETE FROM upload_sessions";
            sql << "DELETE FROM shares";
            sql << "DELETE FROM files";
            sql << "DELETE FROM objects";
        }
    }

    std::unique_ptr<soci::connection_pool> pool_;
    std::string storage_root_;
};

TEST_F(ObjectReclaimerTest, DeletesPendingObjectBytesAndDatabaseRecord) {
    const std::string hash_hex(64, 'a');
    const std::string content_hash(32, static_cast<char>(0xaa));
    const std::string temporary_path = storage_root_ + "/object.tmp";

    {
        std::ofstream temporary_file(temporary_path, std::ios::binary);
        temporary_file << "object bytes";
    }

    filelink::ObjectStore store(storage_root_);
    const std::string object_path = store.commit(temporary_path, hash_hex).objectPath;

    {
        soci::session sql(*pool_);
        filelink::db::ObjectDao objects(sql);
        objects.add_reference(content_hash, 12);
        ASSERT_TRUE(objects.remove_reference(content_hash));
    }

    filelink::ObjectReclaimer reclaimer(*pool_, store);
    EXPECT_EQ(reclaimer.reclaim_pending_objects(), 1);
    EXPECT_EQ(::access(object_path.c_str(), F_OK), -1);

    soci::session sql(*pool_);
    filelink::db::ObjectDao objects(sql);
    filelink::db::Object object;
    EXPECT_FALSE(objects.find(content_hash, object));
}
