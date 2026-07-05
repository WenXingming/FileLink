#pragma once

#include "ObjectStore.h"
#include <string>
#include <cstdint>

namespace soci {
class connection_pool;
}

namespace filelink {

namespace models {
struct UploadSession;
}

class UploadService {
public:
    UploadService(soci::connection_pool& pool, std::string storageRoot, ObjectStore store);
    ~UploadService() = default;

    UploadService(const UploadService&) = delete;
    UploadService& operator=(const UploadService&) = delete;

    /**
     * @brief 获取上传进度
     * @param uploadIdHex 16进制的会话ID
     * @param out_offset 输出当前偏移量
     * @param out_totalSize 输出总大小
     * @return 是否成功找到会话
     */
    bool get_session_progress(const std::string& uploadIdHex, uint64_t& out_offset, uint64_t& out_totalSize);

    /**
     * @brief 获取完整会话以查询状态
     * @param uploadIdHex 16进制的会话ID
     * @param out_session 输出会话模型
     * @return 是否成功找到
     */
    bool get_session(const std::string& uploadIdHex, models::UploadSession& out_session);

    /**
     * @brief 创建一个新的上传会话
     * @param totalSize 文件总大小
     * @param metadataHeader Tus的Upload-Metadata头部内容
     * @param host 客户端发送的Host头部
     * @param out_uploadIdHex 输出生成的16进制会话ID
     * @return 是否成功创建
     */
    bool create_session(uint64_t totalSize, const std::string& metadataHeader, const std::string& host, std::string& out_uploadIdHex);

    /**
     * @brief 追加写入分片数据并推进偏移量
     * @param uploadIdHex 16进制的会话ID
     * @param clientOffset 客户端声明的偏移量
     * @param chunkData 二进制数据块
     * @param out_newOffset 输出写入后最新的偏移量
     * @return 业务状态码 (200 成功, 409 偏移不匹配, 400 参数越界, 404 会话未找到, 500 系统错误)
     */
    int write_session_chunk(const std::string& uploadIdHex, uint64_t clientOffset, const std::string& chunkData, uint64_t& out_newOffset);

private:
    void finalize_session(std::string uploadIdHex);

private:
    soci::connection_pool& pool_;
    std::string storageRoot_;
    ObjectStore store_;
};

} // namespace filelink
