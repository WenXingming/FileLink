// ============================================================================
// 下载业务服务：把已授权的私有文件或分享文件转换为可交付的下载目标。
// 文件所有权和分享有效性仍分别由 FileService、ShareService 负责。
// ============================================================================

#pragma once

#include <string>

namespace filelink {

class FileService;
class ShareService;

struct DownloadTarget {
    std::string object_key;
    std::string display_name;
};

class DownloadService {
public:
    DownloadService(FileService& file_service, ShareService& share_service);

    bool find_private_download(const std::string& owner_user_id, const std::string& file_id, DownloadTarget& target);
    bool find_shared_download(const std::string& token, DownloadTarget& target);

private:
    FileService& file_service_;
    ShareService& share_service_;
};

} // namespace filelink
