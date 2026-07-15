#include "UploadSessionCleaner.h"

#include "database/SociSessionLease.h"

#include <cerrno>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <soci/connection-pool.h>
#include <soci/rowset.h>
#include <soci/soci.h>
#include <sstream>
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

int UploadSessionCleaner::cleanup_terminated_sessions() {
    int successCount = 0;
    try {
        db::SociSessionLease lease(pool_);
        soci::session& sql = lease.get();

        sql << "UPDATE upload_sessions SET state = 'EXPIRED' "
               "WHERE state = 'UPLOADING' AND expires_at < NOW()";

        std::vector<std::string> uploadIds;
        soci::rowset<std::string> rows = (sql.prepare <<
            "SELECT upload_id FROM upload_sessions WHERE state IN ('ABORTED', 'EXPIRED')");
        for (const std::string& uploadId : rows) {
            uploadIds.push_back(uploadId);
        }

        for (const std::string& idBinary : uploadIds) {
            std::string idHex = bytes_to_hex(idBinary);
            std::string partPath = storageRoot_ + "/uploads/" + idHex + ".part";

            if (::unlink(partPath.c_str()) != 0 && errno != ENOENT) {
                std::cerr << "[Cleaner] Warning: Failed to unlink " << partPath
                          << ", error: " << ::strerror(errno) << "\n";
                continue;
            }

            try {
                soci::transaction tr(sql);
                soci::statement statement = (sql.prepare
                    << "DELETE FROM upload_sessions "
                       "WHERE upload_id = :id AND state IN ('ABORTED', 'EXPIRED')",
                    soci::use(idBinary));
                statement.execute(false);
                tr.commit();
                if (statement.get_affected_rows() == 1) {
                    successCount++;
                }
            } catch (const std::exception& e) {
                std::cerr << "[Cleaner] Error: Failed to remove session: "
                          << idHex << ", error: " << e.what() << "\n";
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[Cleaner] Critical Error: Failed to clean terminated sessions: "
                  << e.what() << "\n";
    }

    return successCount;
}

} // namespace filelink
