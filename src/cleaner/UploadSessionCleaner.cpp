#include "UploadSessionCleaner.h"

#include "database/SociSessionLease.h"

#include <cerrno>
#include <iomanip>
#include <iostream>
#include <soci/connection-pool.h>
#include <soci/rowset.h>
#include <soci/soci.h>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace filelink {

namespace {

std::string bytes_to_hex(const std::string& bytes) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned char c : bytes) {
        ss << std::setw(2) << static_cast<int>(c);
    }
    return ss.str();
}

} // namespace

UploadSessionCleaner::UploadSessionCleaner(soci::connection_pool& pool, std::string storageRoot)
    : pool_(pool), storageRoot_(std::move(storageRoot)) {
}

int UploadSessionCleaner::cleanup_expired_sessions() {
    int successCount = 0;
    try {
        db::SociSessionLease lease(pool_);
        soci::session& sql = lease.get();

        // 1. 查询所有已过期的处于 UPLOADING 状态的会话
        soci::rowset<std::string> rows = (sql.prepare <<
            "SELECT upload_id FROM upload_sessions WHERE state = 'UPLOADING' AND expires_at < NOW()");

        // 2. 逐个清理物理文件与数据库状态
        for (const auto& idBinary : rows) {
            std::string idHex = bytes_to_hex(idBinary);
            std::string partPath = storageRoot_ + "/uploads/" + idHex + ".part";

            // 尝试物理删除临时文件
            struct stat st;
            if (::stat(partPath.c_str(), &st) == 0) {
                if (::unlink(partPath.c_str()) != 0 && errno != ENOENT) {
                    std::cerr << "[Cleaner] Warning: Failed to unlink expired file: "
                              << partPath << ", error: " << ::strerror(errno) << "\n";
                }
            }

            // 更新数据库状态为 EXPIRED
            try {
                soci::transaction tr(sql);
                sql << "UPDATE upload_sessions SET state = 'EXPIRED' WHERE upload_id = :id",
                       soci::use(idBinary);
                tr.commit();
                successCount++;
            } catch (const std::exception& e) {
                std::cerr << "[Cleaner] Error: Failed to update database state for session: "
                          << idHex << ", error: " << e.what() << "\n";
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[Cleaner] Critical Error: Database query failed during cleanup: " << e.what() << "\n";
    }

    return successCount;
}

} // namespace filelink
