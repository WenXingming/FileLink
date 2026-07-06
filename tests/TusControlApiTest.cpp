#include "ApiRouter.h"
#include "MySqlTestConfig.h"
#include "DownloadService.h"
#include "StaticFileService.h"
#include "UploadService.h"
#include "tudou/http/HttpServer.h"
#include "tudou/http/HttpRequest.h"
#include "tudou/http/HttpResponse.h"
#include "db/UploadSession.h"
#include <soci/soci.h>
#include <soci/connection-pool.h>
#include <soci/mysql/soci-mysql.h>
#include <gtest/gtest.h>
#include <exception>
#include <memory>
#include <ctime>
#include <fstream>
#include <thread>

using namespace filelink;
using namespace filelink::db;

namespace filelink {

class TusControlApiTest : public ::testing::Test {
protected:
    void call_handle_tus_options(ApiRouter& router, const HttpRequest& req, HttpResponse& resp) {
        router.handle_tus_options(req, resp);
    }
    void call_handle_tus_head(ApiRouter& router, const HttpRequest& req, HttpResponse& resp) {
        router.handle_tus_head(req, resp);
    }
    void call_handle_tus_create(ApiRouter& router, const HttpRequest& req, HttpResponse& resp) {
        router.handle_tus_create(req, resp);
    }
    void call_handle_tus_patch(ApiRouter& router, const HttpRequest& req, HttpResponse& resp) {
        router.handle_tus_patch(req, resp);
    }
    void call_handle_tus_get_session(ApiRouter& router, const HttpRequest& req, HttpResponse& resp) {
        router.handle_tus_get_session(req, resp);
    }
    void call_handle_tus_terminate(ApiRouter& router, const HttpRequest& req, HttpResponse& resp) {
        router.handle_tus_terminate(req, resp);
    }
};

class TusDatabaseApiTest : public TusControlApiTest {
protected:
    void SetUp() override {
        try {
            pool = std::make_unique<soci::connection_pool>(1);
            pool->at(0).open(soci::mysql, test::mysql_connection_string());

            soci::session sql(*pool);
            sql << "DELETE FROM upload_sessions";
        }
        catch (const std::exception& error) {
            pool.reset();
            FAIL() << "MySQL 集成测试初始化失败: " << error.what();
        }
    }

    void TearDown() override {
        if (pool == nullptr) {
            return;
        }
        try {
            soci::session sql(*pool);
            sql << "DELETE FROM upload_sessions";
        }
        catch (const std::exception& error) {
            ADD_FAILURE() << "MySQL 集成测试清理失败: " << error.what();
        }
    }

    std::unique_ptr<soci::connection_pool> pool;
};

TEST_F(TusControlApiTest, OptionsReturnsCapabilities) {
    HttpServer server("127.0.0.1", 9999);
    ObjectStore objectStore("./storage_test");
    DownloadService downloadService(std::move(objectStore));
    StaticFileService staticFileService("./web");

    soci::connection_pool dummyPool(1);
    UploadService uploadService(dummyPool, "./storage_test", ObjectStore("./storage_test"));

    ApiRouter router(server, downloadService, staticFileService, uploadService);

    HttpRequest req;
    req.set_method("OPTIONS");
    req.set_path("/uploads");

    HttpResponse resp;
    call_handle_tus_options(router, req, resp);

    EXPECT_EQ(resp.get_status_code(), 204);
    EXPECT_EQ(resp.get_headers().at("Tus-Resumable"), "1.0.0");
    EXPECT_EQ(resp.get_headers().at("Tus-Version"), "1.0.0");
    EXPECT_FALSE(resp.get_headers().at("Tus-Max-Size").empty());
    EXPECT_EQ(resp.get_headers().at("Tus-Extension"), "creation,expiration");
}

TEST_F(TusDatabaseApiTest, HeadReturnsOffsetForExistingSession) {
    HttpServer server("127.0.0.1", 9999);
    ObjectStore objectStore("./storage_test");
    DownloadService downloadService(std::move(objectStore));
    StaticFileService staticFileService("./web");

    UploadService uploadService(*pool, "./storage_test", ObjectStore("./storage_test"));
    ApiRouter router(server, downloadService, staticFileService, uploadService);

    std::string uploadIdBinary = "\x12\x34\x56\x78\x90\x12\x34\x56\x78\x90\x12\x34\x56\x78\x90\x12";
    std::string uploadIdHex = "12345678901234567890123456789012";

    {
        soci::session sql(*pool);
        UploadSessionDao store(sql);
        UploadSession session;
        session.upload_id = uploadIdBinary;
        session.state = "UPLOADING";
        session.file_name = "test_file.bin";
        session.total_size = 5000;
        session.committed_offset = 1200;
        std::time_t t = std::time(nullptr);
        session.expires_at = *std::localtime(&t);

        store.create(session);
    }

    HttpRequest req;
    req.set_method("HEAD");
    req.set_path("/uploads/" + uploadIdHex);

    HttpResponse resp;
    call_handle_tus_head(router, req, resp);

    EXPECT_EQ(resp.get_status_code(), 200);
    EXPECT_EQ(resp.get_headers().at("Tus-Resumable"), "1.0.0");
    EXPECT_EQ(resp.get_headers().at("Upload-Offset"), "1200");
    EXPECT_EQ(resp.get_headers().at("Upload-Length"), "5000");
}

TEST_F(TusDatabaseApiTest, HeadReturnsNotFoundForNonExistentSession) {
    HttpServer server("127.0.0.1", 9999);
    ObjectStore objectStore("./storage_test");
    DownloadService downloadService(std::move(objectStore));
    StaticFileService staticFileService("./web");

    UploadService uploadService(*pool, "./storage_test", ObjectStore("./storage_test"));
    ApiRouter router(server, downloadService, staticFileService, uploadService);

    HttpRequest req;
    req.set_method("HEAD");
    req.set_path("/uploads/ffffffffffffffffffffffffffffffff");

    HttpResponse resp;
    call_handle_tus_head(router, req, resp);

    EXPECT_EQ(resp.get_status_code(), 404);
}

TEST_F(TusDatabaseApiTest, PostCreatesSessionAndReturns201) {
    HttpServer server("127.0.0.1", 9999);
    ObjectStore objectStore("./storage_test");
    DownloadService downloadService(std::move(objectStore));
    StaticFileService staticFileService("./web");

    UploadService uploadService(*pool, "./storage_test", ObjectStore("./storage_test"));
    ApiRouter router(server, downloadService, staticFileService, uploadService);

    HttpRequest req;
    req.set_method("POST");
    req.set_path("/uploads");
    req.add_header("Upload-Length", "1000000"); // 1MB
    // Upload-Metadata base64 format for filename (test_upload.zip) and expected_hash
    // expected_hash: 64 hex chars base64 encoded -> abcdef... (64 chars) -> Base64(abcdef...)
    // Let's pass filename metadata: "filename dGVzdF91cGxvYWQuemlw" (test_upload.zip in Base64)
    req.add_header("Upload-Metadata", "filename dGVzdF91cGxvYWQuemlw,expected_hash YWJjZGVmMDEyMzQ1Njc4OWFiY2RlZjAxMjM0NTY3ODlhYmNkZWYwMTIzNDU2Nzg5YWJjZGVmMDEyMzQ1Njc4OQ==");

    HttpResponse resp;
    call_handle_tus_create(router, req, resp);

    EXPECT_EQ(resp.get_status_code(), 201);
    EXPECT_EQ(resp.get_headers().at("Tus-Resumable"), "1.0.0");

    std::string location = resp.get_headers().at("Location");
    EXPECT_FALSE(location.empty());

    // Parse UUID Hex from location URL
    std::size_t slashPos = location.find_last_of('/');
    ASSERT_NE(slashPos, std::string::npos);
    std::string uuidHex = location.substr(slashPos + 1);
    EXPECT_EQ(uuidHex.size(), 32); // 32 hex chars

    // Check database
    std::string uuidBinary;
    for (std::size_t i = 0; i < 32; i += 2) {
        char high = uuidHex[i];
        char low = uuidHex[i + 1];
        int h = (high >= 'a') ? (high - 'a' + 10) : ((high >= 'A') ? (high - 'A' + 10) : (high - '0'));
        int l = (low >= 'a') ? (low - 'a' + 10) : ((low >= 'A') ? (low - 'A' + 10) : (low - '0'));
        uuidBinary.push_back(static_cast<char>((h << 4) | l));
    }

    soci::session sql(*pool);
    UploadSessionDao store(sql);
    UploadSession session;
    bool found = store.find(uuidBinary, session);
    ASSERT_TRUE(found);
    EXPECT_EQ(session.state, "UPLOADING");
    EXPECT_EQ(session.file_name, "test_upload.zip");
    EXPECT_EQ(session.total_size, 1000000);
    EXPECT_EQ(session.committed_offset, 0);
    EXPECT_TRUE(session.has_expected_hash);

    EXPECT_EQ(session.expected_hash.size(), 32);
}

TEST_F(TusDatabaseApiTest, PatchUploadsSequenceSuccessfully) {
    HttpServer server("127.0.0.1", 9999);
    ObjectStore objectStore("./storage_test");
    DownloadService downloadService(std::move(objectStore));
    StaticFileService staticFileService("./web");

    UploadService uploadService(*pool, "./storage_test", ObjectStore("./storage_test"));
    ApiRouter router(server, downloadService, staticFileService, uploadService);

    HttpRequest postReq;
    postReq.set_method("POST");
    postReq.set_path("/uploads");
    postReq.add_header("Upload-Length", "24");
    postReq.add_header("Upload-Metadata", "filename dGVzdF9wYXRjaC5iaW4=");

    HttpResponse postResp;
    call_handle_tus_create(router, postReq, postResp);
    ASSERT_EQ(postResp.get_status_code(), 201);

    std::string location = postResp.get_headers().at("Location");
    std::size_t slashPos = location.find_last_of('/');
    std::string uuidHex = location.substr(slashPos + 1);

    HttpRequest patchReq1;
    patchReq1.set_method("PATCH");
    patchReq1.set_path("/uploads/" + uuidHex);
    patchReq1.add_header("Content-Type", "application/offset+octet-stream");
    patchReq1.add_header("Upload-Offset", "0");
    patchReq1.set_body("Hello ");

    HttpResponse patchResp1;
    call_handle_tus_patch(router, patchReq1, patchResp1);
    EXPECT_EQ(patchResp1.get_status_code(), 204);
    EXPECT_EQ(patchResp1.get_headers().at("Upload-Offset"), "6");

    HttpRequest patchReq2;
    patchReq2.set_method("PATCH");
    patchReq2.set_path("/uploads/" + uuidHex);
    patchReq2.add_header("Content-Type", "application/offset+octet-stream");
    patchReq2.add_header("Upload-Offset", "6");
    patchReq2.set_body("World! (18 bytes)_");

    HttpResponse resp2;
    call_handle_tus_patch(router, patchReq2, resp2);
    EXPECT_EQ(resp2.get_status_code(), 204);
    EXPECT_EQ(resp2.get_headers().at("Upload-Offset"), "24");

    HttpRequest getReq;
    getReq.set_method("GET");
    getReq.set_path("/uploads/" + uuidHex);

    HttpResponse getResp;
    bool completed = false;
    std::string contentHashHex;
    for (int i = 0; i < 20; ++i) {
        getResp = HttpResponse();
        call_handle_tus_get_session(router, getReq, getResp);
        if (getResp.get_status_code() == 200) {
            std::string body = getResp.get_body();
            if (body.find(R"("state":"COMPLETED")") != std::string::npos) {
                completed = true;
                std::size_t pos = body.find(R"("content_hash":")");
                if (pos != std::string::npos) {
                    contentHashHex = body.substr(pos + 16, 64);
                }
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    ASSERT_TRUE(completed);
    ASSERT_EQ(contentHashHex.size(), 64);

    std::string objectPath = downloadService.get_object_path(contentHashHex);
    std::ifstream ifs(objectPath, std::ios::binary);
    ASSERT_TRUE(ifs.is_open());
    std::string objectContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    EXPECT_EQ(objectContent, "Hello World! (18 bytes)_");

    ::unlink(objectPath.c_str());
}

TEST_F(TusDatabaseApiTest, PatchUploadsWithServerRestartAndLazyReconstruction) {
    HttpServer server("127.0.0.1", 9999);
    ObjectStore objectStore("./storage_test");
    DownloadService downloadService(std::move(objectStore));
    StaticFileService staticFileService("./web");

    std::string uuidHex;
    // 1. Upload the first chunk with instance 1
    {
        UploadService uploadService(*pool, "./storage_test", ObjectStore("./storage_test"));
        ApiRouter router(server, downloadService, staticFileService, uploadService);

        HttpRequest postReq;
        postReq.set_method("POST");
        postReq.set_path("/uploads");
        postReq.add_header("Upload-Length", "24");
        postReq.add_header("Upload-Metadata", "filename dGVzdF9wYXRjaC5iaW4=");

        HttpResponse postResp;
        call_handle_tus_create(router, postReq, postResp);
        ASSERT_EQ(postResp.get_status_code(), 201);

        std::string location = postResp.get_headers().at("Location");
        std::size_t slashPos = location.find_last_of('/');
        uuidHex = location.substr(slashPos + 1);

        HttpRequest patchReq1;
        patchReq1.set_method("PATCH");
        patchReq1.set_path("/uploads/" + uuidHex);
        patchReq1.add_header("Content-Type", "application/offset+octet-stream");
        patchReq1.add_header("Upload-Offset", "0");
        patchReq1.set_body("Hello ");

        HttpResponse patchResp1;
        call_handle_tus_patch(router, patchReq1, patchResp1);
        EXPECT_EQ(patchResp1.get_status_code(), 204);
        EXPECT_EQ(patchResp1.get_headers().at("Upload-Offset"), "6");
    } // uploadService 1 destroyed, memory cache cleared

    // 2. Upload the rest of the chunks with instance 2, simulating a server restart
    {
        UploadService uploadService(*pool, "./storage_test", ObjectStore("./storage_test"));
        ApiRouter router(server, downloadService, staticFileService, uploadService);

        HttpRequest patchReq2;
        patchReq2.set_method("PATCH");
        patchReq2.set_path("/uploads/" + uuidHex);
        patchReq2.add_header("Content-Type", "application/offset+octet-stream");
        patchReq2.add_header("Upload-Offset", "6");
        patchReq2.set_body("World! (18 bytes)_");

        HttpResponse resp2;
        call_handle_tus_patch(router, patchReq2, resp2);
        EXPECT_EQ(resp2.get_status_code(), 204);
        EXPECT_EQ(resp2.get_headers().at("Upload-Offset"), "24");

        HttpRequest getReq;
        getReq.set_method("GET");
        getReq.set_path("/uploads/" + uuidHex);

        HttpResponse getResp;
        bool completed = false;
        std::string contentHashHex;
        for (int i = 0; i < 20; ++i) {
            getResp = HttpResponse();
            call_handle_tus_get_session(router, getReq, getResp);
            if (getResp.get_status_code() == 200) {
                std::string body = getResp.get_body();
                if (body.find(R"("state":"COMPLETED")") != std::string::npos) {
                    completed = true;
                    std::size_t pos = body.find(R"("content_hash":")");
                    if (pos != std::string::npos) {
                        contentHashHex = body.substr(pos + 16, 64);
                    }
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        ASSERT_TRUE(completed);
        ASSERT_EQ(contentHashHex.size(), 64);

        std::string objectPath = downloadService.get_object_path(contentHashHex);
        std::ifstream ifs(objectPath, std::ios::binary);
        ASSERT_TRUE(ifs.is_open());
        std::string objectContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        EXPECT_EQ(objectContent, "Hello World! (18 bytes)_");

        ::unlink(objectPath.c_str());
    }
}

namespace {
std::string base64_encode_test(const std::string& in) {
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}
}

TEST_F(TusDatabaseApiTest, PostDeduplicationInstantlyCompletes) {
    HttpServer server("127.0.0.1", 9999);
    std::string testStorage = "./storage_test";
    ObjectStore objectStore(testStorage);
    DownloadService downloadService(objectStore);
    StaticFileService staticFileService("./web");
    UploadService uploadService(*pool, testStorage, objectStore);
    ApiRouter router(server, downloadService, staticFileService, uploadService);

    // 1. Prepare object in ObjectStore
    std::string tempFile = testStorage + "/temp_instant_upload.tmp";
    ::mkdir(testStorage.c_str(), 0755);
    std::ofstream ofs(tempFile, std::ios::binary);
    std::string content = "instant_upload_test";
    ofs << content;
    ofs.close();

    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, content.data(), content.size());
    uint8_t hashOutput[BLAKE3_OUT_LEN];
    blake3_hasher_finalize(&hasher, hashOutput, BLAKE3_OUT_LEN);

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < BLAKE3_OUT_LEN; ++i) {
        ss << std::setw(2) << static_cast<int>(hashOutput[i]);
    }
    std::string expectedHash = ss.str();

    CommitResult result = objectStore.commit(tempFile, expectedHash);
    ASSERT_EQ(result.status, CommitStatus::Created);

    // 2. Post creation with expected_hash matching existing object
    HttpRequest postReq;
    postReq.set_method("POST");
    postReq.set_path("/uploads");
    postReq.add_header("Upload-Length", std::to_string(content.size()));
    std::string metadataHeader = "expected_hash " + base64_encode_test(expectedHash);
    postReq.add_header("Upload-Metadata", metadataHeader);

    HttpResponse postResp;
    call_handle_tus_create(router, postReq, postResp);
    ASSERT_EQ(postResp.get_status_code(), 201);

    std::string location = postResp.get_headers().at("Location");
    std::size_t slashPos = location.find_last_of('/');
    std::string uuidHex = location.substr(slashPos + 1);

    // 3. Head check: expect Upload-Offset == total_size (Deduplication successful)
    HttpRequest headReq;
    headReq.set_method("HEAD");
    headReq.set_path("/uploads/" + uuidHex);

    HttpResponse headResp;
    call_handle_tus_head(router, headReq, headResp);
    EXPECT_EQ(headResp.get_status_code(), 200);
    EXPECT_EQ(headResp.get_headers().at("Upload-Offset"), std::to_string(content.size()));
    EXPECT_EQ(headResp.get_headers().at("Upload-Length"), std::to_string(content.size()));

    // 4. Get check: expect state to be COMPLETED
    HttpRequest getReq;
    getReq.set_method("GET");
    getReq.set_path("/uploads/" + uuidHex);

    HttpResponse getResp;
    call_handle_tus_get_session(router, getReq, getResp);
    EXPECT_EQ(getResp.get_status_code(), 200);
    std::string body = getResp.get_body();
    EXPECT_NE(body.find(R"("state":"COMPLETED")"), std::string::npos);

    // Clean up published object
    ::unlink(result.objectPath.c_str());
}

TEST_F(TusDatabaseApiTest, DeleteUploadInstantlyFreesResources) {
    HttpServer server("127.0.0.1", 9999);
    std::string testStorage = "./storage_test";
    ObjectStore objectStore(testStorage);
    DownloadService downloadService(objectStore);
    StaticFileService staticFileService("./web");
    UploadService uploadService(*pool, testStorage, objectStore);
    ApiRouter router(server, downloadService, staticFileService, uploadService);

    // 1. Create upload session
    HttpRequest postReq;
    postReq.set_method("POST");
    postReq.set_path("/uploads");
    postReq.add_header("Upload-Length", "10");
    postReq.add_header("Upload-Metadata", "filename Y2FuY2VsX3Rlc3QuYmlu");

    HttpResponse postResp;
    call_handle_tus_create(router, postReq, postResp);
    ASSERT_EQ(postResp.get_status_code(), 201);

    std::string location = postResp.get_headers().at("Location");
    std::size_t slashPos = location.find_last_of('/');
    std::string uuidHex = location.substr(slashPos + 1);

    // 2. Patch some data to create the physical .part file
    HttpRequest patchReq;
    patchReq.set_method("PATCH");
    patchReq.set_path("/uploads/" + uuidHex);
    patchReq.add_header("Content-Type", "application/offset+octet-stream");
    patchReq.add_header("Upload-Offset", "0");
    patchReq.set_body("Hello");

    HttpResponse patchResp;
    call_handle_tus_patch(router, patchReq, patchResp);
    EXPECT_EQ(patchResp.get_status_code(), 204);

    // Verify .part file exists
    std::string partPath = testStorage + "/uploads/" + uuidHex + ".part";
    struct stat st;
    ASSERT_EQ(::stat(partPath.c_str(), &st), 0);

    // 3. Send DELETE request to cancel upload
    HttpRequest deleteReq;
    deleteReq.set_method("DELETE");
    deleteReq.set_path("/uploads/" + uuidHex);

    HttpResponse deleteResp;
    call_handle_tus_terminate(router, deleteReq, deleteResp);
    EXPECT_EQ(deleteResp.get_status_code(), 204);

    // 4. Verify physical file deleted
    EXPECT_NE(::stat(partPath.c_str(), &st), 0);

    // 5. Verify database state updated to ABORTED
    std::string uuidBinary;
    for (std::size_t i = 0; i < 32; i += 2) {
        char high = uuidHex[i];
        char low = uuidHex[i + 1];
        int h = (high >= 'a') ? (high - 'a' + 10) : ((high >= 'A') ? (high - 'A' + 10) : (high - '0'));
        int l = (low >= 'a') ? (low - 'a' + 10) : ((low >= 'A') ? (low - 'A' + 10) : (low - '0'));
        uuidBinary.push_back(static_cast<char>((h << 4) | l));
    }

    soci::session sql(*pool);
    UploadSessionDao store(sql);
    UploadSession session;
    bool found = store.find(uuidBinary, session);
    ASSERT_TRUE(found);
    EXPECT_EQ(session.state, "ABORTED");

    // 6. Retry DELETE: expect 404 since it's already aborted
    HttpResponse deleteResp2;
    call_handle_tus_terminate(router, deleteReq, deleteResp2);
    EXPECT_EQ(deleteResp2.get_status_code(), 404);
}

} // namespace filelink
