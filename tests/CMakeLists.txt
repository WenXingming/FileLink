# 收集所有测试源文件
file(GLOB_RECURSE FILELINK_TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

# 测试可执行程序
add_executable(filelink_tests ${FILELINK_TEST_SOURCES})

target_link_libraries(filelink_tests PRIVATE
    filelink_lib
    GTest::gtest_main
    soci_core
    soci_mysql
)
target_include_directories(filelink_tests PRIVATE ${soci_SOURCE_DIR}/include ${soci_BINARY_DIR}/include)

# 自动发现 GoogleTest 测试用例
include(GoogleTest)
gtest_discover_tests(filelink_tests)

# 健康检查集成测试
find_program(BASH_EXECUTABLE bash REQUIRED)
find_program(CURL_EXECUTABLE curl REQUIRED)

add_test(
    NAME filelinkServerHealthCheck
    COMMAND
        ${BASH_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/HealthCheckTest.sh
        $<TARGET_FILE:filelink-server>
        ${CURL_EXECUTABLE}
)
set_tests_properties(filelinkServerHealthCheck PROPERTIES TIMEOUT 10)

add_test(
    NAME filelinkServerUploadCheck
    COMMAND
        ${BASH_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/UploadTest.sh
        $<TARGET_FILE:filelink-server>
        ${CURL_EXECUTABLE}
)
set_tests_properties(filelinkServerUploadCheck PROPERTIES TIMEOUT 10)

add_test(
    NAME filelinkServerDownloadCheck
    COMMAND
        ${BASH_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/DownloadTest.sh
        $<TARGET_FILE:filelink-server>
        ${CURL_EXECUTABLE}
)
set_tests_properties(filelinkServerDownloadCheck PROPERTIES TIMEOUT 10)
