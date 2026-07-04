#include "ObjectService.h"
#include "StreamUploader.h"
#include <chrono>
#include <fstream>
#include <sys/stat.h>
#include <stdexcept>

namespace filelink {

ObjectService::ObjectService(ObjectStore store, std::string storageRoot)
    : store_(std::move(store)), storageRoot_(std::move(storageRoot)) {
}

UploadResult ObjectService::process_upload(const std::string& fileBody, const std::string& fileName) {
    uint64_t reqId = ++reqCounter_;
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    std::string tempPath = storageRoot_ + "/tmp_upload_" + std::to_string(now) + "_" + std::to_string(reqId) + ".tmp";

    // 1. 质检打包车间 (StreamUploader)：
    // 在接收数据的同时，一边落盘写入临时文件，一边实时计算 BLAKE3 哈希。如果中途抛出异常，析构函数会自动销毁未完成的临时文件（RAII 安全）。
    StreamUploader uploader(tempPath);
    uploader.append_chunk(fileBody.data(), fileBody.size());
    std::string finalHash = uploader.finalize();

    // 2. 永久智能仓库 (ObjectStore)：
    // 基于内容的原子提交 (Atomic Commit) 与并发去重。内部通过底层的 link() 和 unlink() 确保文件持久化的绝对原子性。
    auto result = store_.commit(tempPath, finalHash);

    std::string ext = "";
    size_t dotPos = fileName.find_last_of('.');
    if (dotPos != std::string::npos) {
        ext = fileName.substr(dotPos);
    }

    UploadResult out;
    out.hash = finalHash;
    out.status = (result.status == CommitStatus::Created ? "created" : "reused");
    out.extension = ext;
    return out;
}

std::string ObjectService::get_object_path(const std::string& contentHash) const {
    return store_.get_object_path(contentHash);
}

std::string ObjectService::get_object_content(const std::string& contentHash) const {
    std::string objectPath = store_.get_object_path(contentHash);

    struct stat info;
    if (::stat(objectPath.c_str(), &info) != 0) {
        throw std::invalid_argument("Object Not Found");
    }

    std::ifstream ifs(objectPath, std::ios::binary);
    if (!ifs) {
        throw std::runtime_error("Failed to open object");
    }

    return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

} // namespace filelink
