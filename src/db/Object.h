#pragma once

#include <soci/soci.h>

#include <cstdint>
#include <string>

namespace filelink {
namespace db {

// ================================================================
// Object：内容寻址存储中唯一、不可变的物理字节对象。
// ================================================================
struct Object {
    std::string content_hash; // BINARY(32) mapped to std::string
    uint64_t byte_size = 0;
    uint64_t ref_count = 0;
    std::string state = "READY";
};

// ===========================================================
// ObjectDao：读写 objects 表中的对象元数据。
// ===========================================================
class ObjectDao {
public:
    explicit ObjectDao(soci::session& sql) : sql_(sql) {}

    void add_reference(const std::string& content_hash, uint64_t byte_size);
    bool find(const std::string& content_hash, Object& out_object);

private:
    soci::session& sql_;
};

} // namespace db
} // namespace filelink
