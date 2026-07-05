#include "ApiRouter.h"
#include "MySqlTestConfig.h"
#include "ObjectService.h"
#include "StaticFileService.h"
#include "UploadService.h"
#include "tudou/http/HttpServer.h"
#include "tudou/http/HttpRequest.h"
#include "tudou/http/HttpResponse.h"
#include "store/UploadSessionStore.h"
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
using namespace filelink::store;
using namespace filelink::models;

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
    ObjectService objectService(std::move(objectStore), "./storage_test");
    StaticFileService staticFileService("./web");

    soci::connection_pool dummyPool(1);
    UploadService uploadService(dummyPool, "./storage_test", ObjectStore("./storage_test"));
    ApiRouter router(server, objectService, staticFileService, uploadService);

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
    ObjectService objectService(std::move(objectStore), "./storage_test");
    StaticFileService staticFileService("./web");

    UploadService uploadService(*pool, "./storage_test", ObjectStore("./storage_test"));
    ApiRouter router(server, objectService, staticFileService, uploadService);

    std::string uploadIdBinary = "\x12\x34\x56\x78\x90\x12\x34\x56\x78\x90\x12\x34\x56\x78\x90\x12";
    std::string uploadIdHex = "12345678901234567890123456789012";

    {
        soci::session sql(*pool);
        UploadSessionStore store(sql);
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
    ObjectService objectService(std::move(objectStore), "./storage_test");
    StaticFileService staticFileService("./web");

    UploadService uploadService(*pool, "./storage_test", ObjectStore("./storage_test"));
    ApiRouter router(server, objectService, staticFileService, uploadService);

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
    ObjectService objectService(std::move(objectStore), "./storage_test");
    StaticFileService staticFileService("./web");

    UploadService uploadService(*pool, "./storage_test", ObjectStore("./storage_test"));
    ApiRouter router(server, objectService, staticFileService, uploadService);

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
    UploadSessionStore store(sql);
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
    ObjectService objectService(std::move(objectStore), "./storage_test");
    StaticFileService staticFileService("./web");

    UploadService uploadService(*pool, "./storage_test", ObjectStore("./storage_test"));
    ApiRouter router(server, objectService, staticFileService, uploadService);

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

    std::string objectPath = objectService.get_object_path(contentHashHex);
    std::ifstream ifs(objectPath, std::ios::binary);
    ASSERT_TRUE(ifs.is_open());
    std::string objectContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    EXPECT_EQ(objectContent, "Hello World! (18 bytes)_");

    ::unlink(objectPath.c_str());
}

} // namespace filelink
