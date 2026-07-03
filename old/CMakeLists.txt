# 旧在线服务的构建入口快照，仅供参考，不参与根项目构建。
cmake_minimum_required(VERSION 3.25)

project(FileLink VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

option(FILELINK_BUILD_DEDUP_TOOL "构建离线去重命令行工具" ON)
option(FILELINK_BUILD_TESTS "构建 FileLink 测试" OFF)
option(FILELINK_BUILD_BENCHMARKS "构建 FileLink 性能基准" OFF)

# 优先使用系统中已安装的 spdlog，未找到时下载固定版本。
if(NOT TARGET spdlog::spdlog)
    find_package(spdlog 1.16 CONFIG QUIET)
endif()

if(NOT TARGET spdlog::spdlog)
    FetchContent_Declare(
        spdlog
        URL https://github.com/gabime/spdlog/archive/refs/tags/v1.16.0.tar.gz
        URL_HASH SHA256=8741753e488a78dd0d0024c980e1fb5b5c85888447e309d9cb9d949bdb52aa3e
    )

    function(filelink_fetch_spdlog)
        set(BUILD_SHARED_LIBS OFF)
        set(SPDLOG_BUILD_EXAMPLE OFF)
        set(SPDLOG_BUILD_TESTS OFF)
        set(SPDLOG_BUILD_BENCH OFF)
        set(SPDLOG_INSTALL OFF)
        FetchContent_MakeAvailable(spdlog)
    endfunction()

    filelink_fetch_spdlog()
endif()

# 优先使用已安装的 Tudou，未找到时直接从源码仓库获取。
if(NOT TARGET Tudou::tudou)
    find_package(Tudou CONFIG QUIET)
endif()

if(NOT TARGET Tudou::tudou)
    FetchContent_Declare(
        Tudou
        GIT_REPOSITORY https://github.com/WenXingming/Tudou.git
        GIT_TAG main
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Tudou)
endif()

# dedup 引擎使用独立线程池执行文件扫描与哈希任务。
if(NOT TARGET ThreadPool)
    find_package(ThreadPool CONFIG QUIET)
endif()

if(NOT TARGET ThreadPool)
    FetchContent_Declare(
        ThreadPool
        GIT_REPOSITORY https://github.com/WenXingming/ThreadPool.git
        GIT_TAG main
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(ThreadPool)
endif()

# 优先使用系统中的 BLAKE3，未找到时获取官方 C 实现。
if(NOT TARGET BLAKE3::blake3 AND NOT TARGET blake3)
    find_package(blake3 CONFIG QUIET)
endif()

if(NOT TARGET BLAKE3::blake3 AND NOT TARGET blake3)
    FetchContent_Declare(
        blake3
        GIT_REPOSITORY https://github.com/BLAKE3-team/BLAKE3.git
        GIT_TAG 1.5.1
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR c
    )
    FetchContent_MakeAvailable(blake3)
endif()

if(TARGET BLAKE3::blake3)
    set(FILELINK_BLAKE3_TARGET BLAKE3::blake3)
else()
    set(FILELINK_BLAKE3_TARGET blake3)
endif()

# CLI11 只服务于离线工具，不进入 dedup 核心库。
if(FILELINK_BUILD_DEDUP_TOOL AND NOT TARGET CLI11::CLI11)
    find_package(CLI11 2.4 CONFIG QUIET)
endif()

if(FILELINK_BUILD_DEDUP_TOOL AND NOT TARGET CLI11::CLI11)
    FetchContent_Declare(
        CLI11
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG v2.4.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(CLI11)
endif()

add_executable(filelink-server
    src/ConfigLoader.cpp
    src/FileLinkServer.cpp
    src/FileLinkService.cpp
    src/auth/AuthService.cpp
    src/filestore/FileSystemStorage.cpp
    src/main.cpp
    src/metacache/RedisFileMetaCache.cpp
    src/metastore/InMemoryFileMetaStore.cpp
    src/metastore/MysqlFileMetaStore.cpp
)

target_include_directories(filelink-server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# OpenSSL 为内容寻址存储提供 SHA-256 能力。
find_package(OpenSSL REQUIRED)

target_link_libraries(filelink-server PRIVATE
    Tudou::tudou
    spdlog::spdlog
    OpenSSL::Crypto
)

# MySQL 支持是可选能力，未安装连接器时退化为内存元数据存储。
find_path(MYSQLCPPCONN_INCLUDE_DIR NAMES mysql_driver.h)
find_library(MYSQLCPPCONN_LIBRARY NAMES mysqlcppconn mysqlcppconn8)

if(MYSQLCPPCONN_INCLUDE_DIR AND MYSQLCPPCONN_LIBRARY)
    target_include_directories(filelink-server PRIVATE ${MYSQLCPPCONN_INCLUDE_DIR})
    target_link_libraries(filelink-server PRIVATE ${MYSQLCPPCONN_LIBRARY})
    target_compile_definitions(filelink-server PRIVATE FILELINK_WITH_MYSQLCPPCONN=1)
else()
    message(STATUS "未找到 mysql-connector-c++，FileLink 将使用内存元数据存储")
    target_compile_definitions(filelink-server PRIVATE FILELINK_WITH_MYSQLCPPCONN=0)
endif()

# Redis 支持是可选能力，未安装 hiredis 时退化为空缓存实现。
find_path(HIREDIS_INCLUDE_DIR NAMES hiredis/hiredis.h)
find_library(HIREDIS_LIBRARY NAMES hiredis)

if(HIREDIS_INCLUDE_DIR AND HIREDIS_LIBRARY)
    target_include_directories(filelink-server PRIVATE ${HIREDIS_INCLUDE_DIR})
    target_link_libraries(filelink-server PRIVATE ${HIREDIS_LIBRARY})
    target_compile_definitions(filelink-server PRIVATE FILELINK_WITH_HIREDIS=1)
else()
    message(STATUS "未找到 hiredis，FileLink 将不启用 Redis 缓存")
    target_compile_definitions(filelink-server PRIVATE FILELINK_WITH_HIREDIS=0)
endif()

add_subdirectory(src/dedup)

if(FILELINK_BUILD_DEDUP_TOOL)
    add_subdirectory(tools/dedup)
endif()

if(FILELINK_BUILD_TESTS)
    enable_testing()

    if(NOT TARGET GTest::gtest_main)
        find_package(GTest CONFIG QUIET)
    endif()

    if(NOT TARGET GTest::gtest_main)
        FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        )
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

    add_subdirectory(tests/dedup)
endif()

if(FILELINK_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks/dedup)
endif()
