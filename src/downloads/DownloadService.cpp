// ============================================================================
// 下载业务服务实现：先让所属领域验证访问权限，再定位内容寻址对象。
// 不解析 HTTP，也不构造响应或读取文件内容。
// ============================================================================

#include "DownloadService.h"

#include "files/FileService.h"
#include "shares/ShareService.h"
#include "storage/ObjectStore.h"

#include <iomanip>
#include <sstream>

namespace filelink {

namespace {

std::string hex_encode(const std::string& bytes) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (unsigned char value : bytes) {
        stream << std::setw(2) << static_cast<int>(value);
    }
    return stream.str();
}

} // namespace

DownloadService::DownloadService(FileService& file_service, ShareService& share_service)
    : file_service_(file_service), share_service_(share_service) {}

bool DownloadService::find_private_download(const std::string& owner_user_id,
    const std::string& file_id, DownloadTarget& target) {
    db::File file;
    if (!file_service_.find_file(owner_user_id, file_id, file)) {
        return false;
    }
    target = {ObjectStore::get_object_key(hex_encode(file.content_hash)), file.display_name};
    return true;
}

bool DownloadService::find_shared_download(const std::string& token,
    DownloadTarget& target) {
    db::File file;
    if (!share_service_.find_shared_file(token, file)) {
        return false;
    }
    target = {ObjectStore::get_object_key(hex_encode(file.content_hash)), file.display_name};
    return true;
}

} // namespace filelink
