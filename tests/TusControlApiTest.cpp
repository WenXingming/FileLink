#include "MySqlTestConfig.h"
#include "site/StaticFileService.h"
#include "auth/AuthService.h"
#include "auth/RequestAuthenticator.h"
#include "uploads/UploadApiRouter.h"
#include "uploads/UploadService.h"
#include "tudou/http/HttpServer.h"
#include "tudou/http/HttpRequest.h"
#include "tudou/http/HttpResponse.h"
#include "database/UploadSession.h"
#include "database/File.h"
#include "database/Object.h"
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

using ApiRouter = UploadApiRouter;

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
    void call_handle_tus_head_as(ApiRouter& router, HttpRequest req, HttpResponse& resp,
        const std::string& session_token) {
        req.add_header("Cookie", "filelink_session=" + session_token);
        router.handle_tus_head(req, resp);
    }
    void call_handle_tus_patch_as(ApiRouter& router, HttpRequest req, HttpResponse& resp,
        const std::string& session_token) {
        req.add_header("Cookie", "filelink_session=" + session_token);
        router.handle_tus_patch(req, resp);
    }
    void call_handle_tus_get_session_as(ApiRouter& router, HttpRequest req, HttpResponse& resp,
        const std::string& session_token) {
        req.add_header("Cookie", "filelink_session=" + session_token);
        router.handle_tus_get_session(req, resp);
    }
    void call_handle_tus_terminate_as(ApiRouter& router, HttpRequest req, HttpResponse& resp,
        const std::string& session_token) {
        req.add_header("Cookie", "filelink_session=" + session_token);
        router.handle_tus_terminate(req, resp);
    }
};

class TusDatabaseApiTest : public TusControlApiTest {
protected:
    void SetUp() override {
        try {
            pool = std::make_unique<soci::connection_pool>(1);
            pool->at(0).open(soci::mysql, test::mysql_connection_string());

            {
                soci::session sql(*pool);
                sql << "DELETE FROM upload_sessions";
                sql << "DELETE FROM shares";
                sql << "DELETE FROM files";
                sql << "DELETE FROM objects";
                sql << "DELETE FROM user_sessions";
                sql << "DELETE FROM users";
            }

            authService = std::make_unique<AuthService>(*pool);
            if (authService->register_user("tus_owner", "correct-password", ownerSession)
                != RegisterResult::Success) {
                throw std::runtime_error("failed to create TUS test owner");
            }
            requestAuthenticator = std::make_unique<RequestAuthenticator>(*authService);
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
            sql << "DELETE FROM shares";
            sql << "DELETE FROM files";
            sql << "DELETE FROM objects";
            sql << "DELETE FROM user_sessions";
            sql << "DELETE FROM users";
        }
        catch (const std::exception& error) {
            ADD_FAILURE() << "MySQL 集成测试清理失败: " << error.what();
        }
    }

    std::unique_ptr<soci::connection_pool> pool;
    std::unique_ptr<AuthService> authService;
    std::unique_ptr<RequestAuthenticator> requestAuthenticator;
    AuthenticatedSession ownerSession;

    void authenticate(HttpRequest& request) const {
        request.add_header("Cookie", "filelink_session=" + ownerSession.session_token);
    }

    void call_handle_tus_head(ApiRouter& router, const HttpRequest& request, HttpResponse& response) {
        HttpRequest authenticated = request;
        authenticate(authenticated);
        TusControlApiTest::call_handle_tus_head(router, authenticated, response);
    }

    void call_handle_tus_patch(ApiRouter& router, const HttpRequest& request, HttpResponse& response) {
        HttpRequest authenticated = request;
        authenticate(authenticated);
        TusControlApiTest::call_handle_tus_patch(router, authenticated, response);
    }

    void call_handle_tus_get_session(ApiRouter& router, const HttpRequest& request, HttpResponse& response) {
        HttpRequest authenticated = request;
        authenticate(authenticated);
        TusControlApiTest::call_handle_tus_get_session(router, authenticated, response);
    }

    void call_handle_tus_terminate(ApiRouter& router, const HttpRequest& request, HttpResponse& response) {
        HttpRequest authenticated = request;
        authenticate(authenticated);
        TusControlApiTest::call_handle_tus_terminate(router, authenticated, response);
    }
};

TEST_F(TusControlApiTest, OptionsReturnsCapabilities) {
    HttpServer server("127.0.0.1", 9999);
    soci::connection_pool dummyPool(1);
    UploadService uploadService(dummyPool, "./storage_test", ObjectStore("./storage_test"));
    AuthService authService(dummyPool);
    RequestAuthenticator requestAuthenticator(authService);

    ApiRouter router(server, uploadService, requestAuthenticator);

    HttpRequest req;
    req.set_method("OPTIONS");
    req.set_path("/uploads");

    HttpResponse resp;
    call_handle_tus_options(router, req, resp);

    EXPECT_EQ(resp.get_status_code(), 204);
    EXPECT_EQ(resp.get_headers().at("Tus-Resumable"), "1.0.0");
    EXPECT_EQ(resp.get_headers().at("Tus-Version"), "1.0.0");
    EXPECT_FALSE(resp.get_headers().at("Tus-Max-Size").empty());
    EXPECT_EQ(resp.get_headers().at("Tus-Extension"), "creation,expiration,termination");
}

TEST_F(TusDatabaseApiTest, HeadReturnsOffsetForExistingSession) {
    HttpServer server("127.0.0.1", 9999);
    UploadService uploadService(*pool, "./storage_test", ObjectStore("./storage_test"));
    ApiRouter router(server, uploadService, *requestAuthenticator);

    std::string uploadIdBinary = "\x12\x34\x56\x78\x90\x12\x34\x56\x78\x90\x12\x34\x56\x78\x90\x12";
    std::string uploadIdHex = "12345678901234567890123456789012";

    {
        soci::session sql(*pool);
        UploadSessionDao store(sql);
        UploadSession session;
        session.upload_id = uploadIdBinary;
        session.owner_user_id = ownerSession.user_id;
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
    UploadService uploadService(*pool, "./storage_test", ObjectStore("./storage_test"));
    ApiRouter router(server, uploadService, *requestAuthenticator);

    HttpRequest req;
    req.set_method("HEAD");
    req.set_path("/uploads/ffffffffffffffffffffffffffffffff");

    HttpResponse resp;
    call_handle_tus_head(router, req, resp);

    EXPECT_EQ(resp.get_status_code(), 404);
}

TEST_F(TusDatabaseApiTest, PostCreatesSessionAndReturns201) {
    HttpServer server("127.0.0.1", 9999);
    UploadService uploadService(*pool, "./storage_test", ObjectStore("./storage_test"));
    ApiRouter router(server, uploadService, *requestAuthenticator);

    HttpRequest req;
    req.set_method("POST");
    req.set_path("/uploads");
    authenticate(req);
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
    EXPECT_EQ(session.owner_user_id, ownerSession.user_id);
    EXPECT_EQ(session.file_name, "test_upload.zip");
    EXPECT_EQ(session.total_size, 1000000);
    EXPECT_EQ(session.committed_offset, 0);
    EXPECT_TRUE(session.has_expected_hash);

    EXPECT_EQ(session.expected_hash.size(), 32);
}

TEST_F(TusDatabaseApiTest, PostRejectsUnauthenticatedUpload) {
    HttpServer server("127.0.0.1", 9999);
    UploadService uploadService(*pool, "./storage_test", ObjectStore("./storage_test"));
    ApiRouter router(server, uploadService, *requestAuthenticator);

    HttpRequest request;
    request.set_method("POST");
    request.set_path("/uploads");
    request.add_header("Upload-Length", "24");

    HttpResponse response;
    call_handle_tus_create(router, request, response);

    EXPECT_EQ(response.get_status_code(), 401);
}

TEST_F(TusDatabaseApiTest, HidesAnotherUsersUploadSession) {
    HttpServer server("127.0.0.1", 9999);
    UploadService uploadService(*pool, "./storage_test", ObjectStore("./storage_test"));
    ApiRouter router(server, uploadService, *requestAuthenticator);

    HttpRequest create_request;
    create_request.set_method("POST");
    create_request.set_path("/uploads");
    authenticate(create_request);
    create_request.add_header("Upload-Length", "24");
    HttpResponse create_response;
    call_handle_tus_create(router, create_request, create_response);
    ASSERT_EQ(create_response.get_status_code(), 201);
    const std::string location = create_response.get_headers().at("Location");
    const std::string upload_id = location.substr(location.find_last_of('/') + 1);

    AuthenticatedSession intruder;
    ASSERT_EQ(authService->register_user("tus_intruder", "correct-password", intruder),
        RegisterResult::Success);

    HttpRequest head_request;
    head_request.set_method("HEAD");
    head_request.set_path("/uploads/" + upload_id);
    HttpResponse head_response;
    call_handle_tus_head_as(router, head_request, head_response, intruder.session_token);
    EXPECT_EQ(head_response.get_status_code(), 404);

    HttpRequest patch_request;
    patch_request.set_method("PATCH");
    patch_request.set_path("/uploads/" + upload_id);
    patch_request.add_header("Content-Type", "application/offset+octet-stream");
    patch_request.add_header("Upload-Offset", "0");
    patch_request.set_body("blocked");
    HttpResponse patch_response;
    call_handle_tus_patch_as(router, patch_request, patch_response, intruder.session_token);
    EXPECT_EQ(patch_response.get_status_code(), 404);

    HttpRequest get_request;
    get_request.set_method("GET");
    get_request.set_path("/uploads/" + upload_id);
    HttpResponse get_response;
    call_handle_tus_get_session_as(router, get_request, get_response, intruder.session_token);
    EXPECT_EQ(get_response.get_status_code(), 404);

    HttpRequest delete_request;
    delete_request.set_method("DELETE");
    delete_request.set_path("/uploads/" + upload_id);
    HttpResponse delete_response;
    call_handle_tus_terminate_as(router, delete_request, delete_response, intruder.session_token);
    EXPECT_EQ(delete_response.get_status_code(), 404);
}

TEST_F(TusDatabaseApiTest, PatchUploadsSequenceSuccessfully) {
    HttpServer server("127.0.0.1", 9999);
    UploadService uploadService(*pool, "./storage_test", ObjectStore("./storage_test"));
    ApiRouter router(server, uploadService, *requestAuthenticator);

    HttpRequest postReq;
    postReq.set_method("POST");
    postReq.set_path("/uploads");
    authenticate(postReq);
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
    std::string fileIdHex;
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
                pos = body.find(R"("file_id":")");
                if (pos != std::string::npos) {
                    fileIdHex = body.substr(pos + 11, 32);
                }
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    ASSERT_TRUE(completed);
    ASSERT_EQ(contentHashHex.size(), 64);
    ASSERT_EQ(fileIdHex.size(), 32);

    std::string objectPath = ObjectStore("./storage_test").get_object_path(contentHashHex);
    std::ifstream ifs(objectPath, std::ios::binary);
    ASSERT_TRUE(ifs.is_open());
    std::string objectContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    EXPECT_EQ(objectContent, "Hello World! (18 bytes)_");

    {
        soci::session sql(*pool);
        std::vector<File> files;
        FileDao(sql).find_by_owner(ownerSession.user_id, files);
        ASSERT_EQ(files.size(), 1u);
        EXPECT_EQ(files[0].display_name, "test_patch.bin");

        Object object;
        ASSERT_TRUE(ObjectDao(sql).find(files[0].content_hash, object));
        EXPECT_EQ(object.ref_count, 1u);
    }

    ::unlink(objectPath.c_str());
}

TEST_F(TusDatabaseApiTest, PatchUploadsWithServerRestartAndLazyReconstruction) {
    HttpServer server("127.0.0.1", 9999);
    std::string uuidHex;
    // 1. Upload the first chunk with instance 1
    {
        UploadService uploadService(*pool, "./storage_test", ObjectStore("./storage_test"));
    ApiRouter router(server, uploadService, *requestAuthenticator);

        HttpRequest postReq;
        postReq.set_method("POST");
        postReq.set_path("/uploads");
        authenticate(postReq);
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
    ApiRouter router(server, uploadService, *requestAuthenticator);

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

        std::string objectPath = ObjectStore("./storage_test").get_object_path(contentHashHex);
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

std::string hex_to_bytes_test(const std::string& hex) {
    std::string bytes;
    bytes.reserve(hex.size() / 2);
    for (std::size_t index = 0; index < hex.size(); index += 2) {
        const char high = hex[index];
        const char low = hex[index + 1];
        const int upper = high >= 'a' ? high - 'a' + 10 : high - '0';
        const int lower = low >= 'a' ? low - 'a' + 10 : low - '0';
        bytes.push_back(static_cast<char>((upper << 4) | lower));
    }
    return bytes;
}
}

TEST_F(TusDatabaseApiTest, PostDeduplicationInstantlyCompletes) {
    HttpServer server("127.0.0.1", 9999);
    std::string testStorage = "./storage_test";
    ObjectStore objectStore(testStorage);
    UploadService uploadService(*pool, testStorage, objectStore);
    ApiRouter router(server, uploadService, *requestAuthenticator);

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
    authenticate(postReq);
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

    HttpResponse second_post_response;
    call_handle_tus_create(router, postReq, second_post_response);
    ASSERT_EQ(second_post_response.get_status_code(), 201);

    // 5. The completed upload owns a logical file and references the object.
    {
        soci::session sql(*pool);
        std::vector<File> files;
        FileDao(sql).find_by_owner(ownerSession.user_id, files);
        ASSERT_EQ(files.size(), 2u);
        EXPECT_EQ(files[0].content_hash, hex_to_bytes_test(expectedHash));

        Object object;
        ASSERT_TRUE(ObjectDao(sql).find(files[0].content_hash, object));
        EXPECT_EQ(object.ref_count, 2u);
    }

    // Clean up published object
    ::unlink(result.objectPath.c_str());
}

TEST_F(TusDatabaseApiTest, DeleteUploadInstantlyFreesResources) {
    HttpServer server("127.0.0.1", 9999);
    std::string testStorage = "./storage_test";
    ObjectStore objectStore(testStorage);
    UploadService uploadService(*pool, testStorage, objectStore);
    ApiRouter router(server, uploadService, *requestAuthenticator);

    // 1. Create upload session
    HttpRequest postReq;
    postReq.set_method("POST");
    postReq.set_path("/uploads");
    authenticate(postReq);
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

    {
        soci::session sql(*pool);
        UploadSessionDao store(sql);
        UploadSession session;
        bool found = store.find(uuidBinary, session);
        ASSERT_TRUE(found);
        EXPECT_EQ(session.state, "ABORTED");
    }

    // 6. Retry DELETE: expect 404 since it's already aborted
    HttpResponse deleteResp2;
    call_handle_tus_terminate(router, deleteReq, deleteResp2);
    EXPECT_EQ(deleteResp2.get_status_code(), 404);
}

TEST_F(TusDatabaseApiTest, DeleteRejectsFinalizingUpload) {
    HttpServer server("127.0.0.1", 9999);
    ObjectStore objectStore("./storage_test");
    UploadService uploadService(*pool, "./storage_test", objectStore);
    ApiRouter router(server, uploadService, *requestAuthenticator);

    HttpRequest post_request;
    post_request.set_method("POST");
    post_request.set_path("/uploads");
    authenticate(post_request);
    post_request.add_header("Upload-Length", "10");

    HttpResponse post_response;
    call_handle_tus_create(router, post_request, post_response);
    ASSERT_EQ(post_response.get_status_code(), 201);

    {
        soci::session sql(*pool);
        sql << "UPDATE upload_sessions SET state = 'FINALIZING' WHERE owner_user_id = :owner",
            soci::use(ownerSession.user_id);
    }

    const std::string location = post_response.get_headers().at("Location");
    const std::string upload_id = location.substr(location.find_last_of('/') + 1);
    HttpRequest delete_request;
    delete_request.set_method("DELETE");
    delete_request.set_path("/uploads/" + upload_id);

    HttpResponse delete_response;
    call_handle_tus_terminate(router, delete_request, delete_response);
    EXPECT_EQ(delete_response.get_status_code(), 409);
}

TEST_F(TusDatabaseApiTest, PostDeduplicationRejectsIncorrectSize) {
    HttpServer server("127.0.0.1", 9999);
    std::string testStorage = "./storage_test";
    ObjectStore objectStore(testStorage);
    UploadService uploadService(*pool, testStorage, objectStore);
    ApiRouter router(server, uploadService, *requestAuthenticator);

    // 1. Prepare object in ObjectStore with content "damaged" (7 bytes)
    std::string tempFile = testStorage + "/temp_size_mismatch.tmp";
    ::mkdir(testStorage.c_str(), 0755);
    std::ofstream ofs(tempFile, std::ios::binary);
    std::string content = "damaged";
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

    // 2. Post creation with expected_hash matching existing object,
    //    BUT specify a different Upload-Length (100 bytes, mismatched from 7 bytes).
    HttpRequest postReq;
    postReq.set_method("POST");
    postReq.set_path("/uploads");
    authenticate(postReq);
    postReq.add_header("Upload-Length", "100"); // 100 != 7
    std::string metadataHeader = "expected_hash " + base64_encode_test(expectedHash);
    postReq.add_header("Upload-Metadata", metadataHeader);

    HttpResponse postResp;
    call_handle_tus_create(router, postReq, postResp);
    ASSERT_EQ(postResp.get_status_code(), 201); // Created successfully, but should NOT hit deduplication

    std::string location = postResp.get_headers().at("Location");
    std::size_t slashPos = location.find_last_of('/');
    std::string uuidHex = location.substr(slashPos + 1);

    // 3. Head check: expect Upload-Offset == 0, state == UPLOADING
    HttpRequest headReq;
    headReq.set_method("HEAD");
    headReq.set_path("/uploads/" + uuidHex);

    HttpResponse headResp;
    call_handle_tus_head(router, headReq, headResp);
    EXPECT_EQ(headResp.get_status_code(), 200);
    EXPECT_EQ(headResp.get_headers().at("Upload-Offset"), "0"); // Not completed!
    EXPECT_EQ(headResp.get_headers().at("Upload-Length"), "100");

    // Clean up
    std::string partPath = testStorage + "/uploads/" + uuidHex + ".part";
    ::unlink(partPath.c_str());
    ::unlink(result.objectPath.c_str());
}

} // namespace filelink
