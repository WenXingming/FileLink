include(GoogleTest)

if(FILELINK_BUILD_TESTS)
    add_executable(filelink_unit_tests
        AppConfigTests.cpp
        ObjectStoreTests.cpp
        PasswordHasherTests.cpp
    )
    target_link_libraries(filelink_unit_tests PRIVATE
        filelink_lib
        GTest::gtest_main
    )
    gtest_discover_tests(
        filelink_unit_tests
        TEST_PREFIX "unit."
        PROPERTIES LABELS "unit"
    )
endif()

if(FILELINK_BUILD_INTEGRATION_TESTS)
    add_executable(filelink_integration_tests
        TusControlApiTest.cpp
        UploadSessionDaoTest.cpp
        ObjectDaoTest.cpp
        FileDaoTest.cpp
        ShareDaoTest.cpp
        ShareServiceTest.cpp
        ShareApiTest.cpp
        FileApiTest.cpp
        UserDaoTest.cpp
        UserSessionDaoTest.cpp
        AuthServiceTest.cpp
        AuthApiTest.cpp
        UserSessionCacheTest.cpp
        ObjectOrphanReclaimerTest.cpp
        ObjectReclaimerTest.cpp
    )
    target_link_libraries(filelink_integration_tests PRIVATE
        filelink_lib
        GTest::gtest_main
    )
    gtest_discover_tests(
        filelink_integration_tests
        TEST_PREFIX "integration."
        PROPERTIES LABELS "integration" RUN_SERIAL TRUE
    )

    find_program(BASH_EXECUTABLE bash REQUIRED)
    find_program(CURL_EXECUTABLE curl REQUIRED)
    find_program(DOCKER_EXECUTABLE docker REQUIRED)

    add_test(
        NAME integration.filelinkServerHealthCheck
        COMMAND
            ${BASH_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/HealthCheckTest.sh
            $<TARGET_FILE:filelink-server>
            ${CURL_EXECUTABLE}
    )
    add_test(
        NAME integration.filelinkServerUploadCheck
        COMMAND
            ${BASH_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/UploadTest.sh
            $<TARGET_FILE:filelink-server>
            ${CURL_EXECUTABLE}
    )
    add_test(
        NAME integration.filelinkServerDownloadCheck
        COMMAND
            ${BASH_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/DownloadTest.sh
            $<TARGET_FILE:filelink-server>
            ${CURL_EXECUTABLE}
            ${DOCKER_EXECUTABLE}
    )
    set_tests_properties(
        integration.filelinkServerHealthCheck
        integration.filelinkServerUploadCheck
        integration.filelinkServerDownloadCheck
        PROPERTIES LABELS "integration" RUN_SERIAL TRUE TIMEOUT 10
    )
endif()
