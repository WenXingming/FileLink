#include "MySqlTestConfig.h"
#include "ObjectStore.h"
#include "cleaner/ObjectOrphanReclaimer.h"
#include "db/Object.h"

#include <gtest/gtest.h>

#include <fstream>
#include <memory>
#include <soci/connection-pool.h>
#include <soci/mysql/soci-mysql.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

class ObjectOrphanReclaimerTest : public testing::Test {
protected:
    void SetUp() override {
        pool_ = std::make_unique<soci::connection_pool>(1);
        pool_->at(0).open(soci::mysql, filelink::test::mysql_connection_string());
        clear_objects();

        storage_root_ = "./storage_test_orphan_scanner";
        ::mkdir(storage_root_.c_str(), 0755);
    }

    void TearDown() override {
        clear_objects();
        remove_object("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
        remove_object("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
        ::rmdir((storage_root_ + "/objects").c_str());
        ::rmdir(storage_root_.c_str());
    }

    void clear_objects() {
        if (pool_ != nullptr) {
            soci::session sql(*pool_);
            sql << "DELETE FROM upload_sessions";
            sql << "DELETE FROM files";
            sql << "DELETE FROM objects";
        }
    }

    void remove_object(const std::string& hash_hex) {
        const std::string directory = storage_root_ + "/objects/"
            + hash_hex.substr(0, 2) + "/" + hash_hex.substr(2, 2);
        ::unlink((directory + "/" + hash_hex).c_str());
        ::rmdir(directory.c_str());
        ::rmdir((storage_root_ + "/objects/" + hash_hex.substr(0, 2)).c_str());
    }

    std::unique_ptr<soci::connection_pool> pool_;
    std::string storage_root_;
};

TEST_F(ObjectOrphanReclaimerTest, FindsUnregisteredObjectWithoutDeletingIt) {
    const std::string registered_hash(64, 'a');
    const std::string orphaned_hash(64, 'b');
    const std::string registered_content_hash(32, static_cast<char>(0xaa));
    const std::string registered_temp_path = storage_root_ + "/registered.tmp";
    const std::string orphaned_temp_path = storage_root_ + "/orphaned.tmp";

    {
        std::ofstream registered_file(registered_temp_path, std::ios::binary);
        registered_file << "registered bytes";
        std::ofstream orphaned_file(orphaned_temp_path, std::ios::binary);
        orphaned_file << "orphaned bytes";
    }

    filelink::ObjectStore store(storage_root_);
    const std::string registered_path = store.commit(registered_temp_path, registered_hash).objectPath;
    const std::string orphaned_path = store.commit(orphaned_temp_path, orphaned_hash).objectPath;

    {
        soci::session sql(*pool_);
        filelink::db::ObjectDao objects(sql);
        objects.add_reference(registered_content_hash, 16);
    }

    filelink::ObjectOrphanReclaimer reclaimer(*pool_, storage_root_);
    const std::vector<std::string> paths = reclaimer.find_orphaned_object_paths();

    EXPECT_EQ(paths, std::vector<std::string>{ orphaned_path });
    EXPECT_EQ(::access(registered_path.c_str(), F_OK), 0);
    EXPECT_EQ(::access(orphaned_path.c_str(), F_OK), 0);
}

TEST_F(ObjectOrphanReclaimerTest, ReclaimsUnregisteredObject) {
    const std::string orphaned_hash(64, 'b');
    const std::string orphaned_temp_path = storage_root_ + "/orphaned.tmp";

    {
        std::ofstream orphaned_file(orphaned_temp_path, std::ios::binary);
        orphaned_file << "orphaned bytes";
    }

    filelink::ObjectStore store(storage_root_);
    const std::string orphaned_path = store.commit(orphaned_temp_path, orphaned_hash).objectPath;

    filelink::ObjectOrphanReclaimer reclaimer(*pool_, storage_root_);
    EXPECT_EQ(reclaimer.reclaim_orphaned_objects(), 1);
    EXPECT_EQ(::access(orphaned_path.c_str(), F_OK), -1);
}
