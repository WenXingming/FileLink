cmake_minimum_required(VERSION 3.25)

project(FileLink VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

option(FILELINK_BUILD_DEDUP_TOOL "构建离线去重命令行工具" ON)
option(FILELINK_BUILD_TESTS "构建当前稳定模块的测试" OFF)
option(FILELINK_BUILD_BENCHMARKS "构建当前稳定模块的性能基准" OFF)

# dedup 引擎使用独立线程池执行文件扫描与哈希任务。
if(NOT TARGET ThreadPool)
    find_package(ThreadPool CONFIG QUIET)
endif()

if(NOT TARGET ThreadPool)
    FetchContent_Declare(
        ThreadPool
        GIT_REPOSITORY https://github.com/WenXingming/ThreadPool.git
        GIT_TAG master
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

# 配置模块和离线工具共享 CLI11；优先使用系统安装，未找到时自动获取。
if(NOT TARGET CLI11::CLI11)
    find_package(CLI11 2.4 CONFIG QUIET)
endif()

if(NOT TARGET CLI11::CLI11)
    FetchContent_Declare(
        CLI11
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG v2.4.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(CLI11)
endif()

# 在线服务依赖 Tudou；优先使用系统安装，未找到时获取当前开发分支。
if(NOT TARGET Tudou::tudou)
    find_package(Tudou CONFIG QUIET)
endif()

if(NOT TARGET Tudou::tudou)
    FetchContent_Declare(
        Tudou
        GIT_REPOSITORY https://github.com/WenXingming/Tudou.git
        GIT_TAG develop
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Tudou)
endif()

add_subdirectory(src/config)
add_subdirectory(src/dedup)
add_subdirectory(src/storage)
add_subdirectory(src/server)

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

    add_subdirectory(tests/config)
    add_subdirectory(tests/dedup)
    add_subdirectory(tests/storage)
    add_subdirectory(tests/server)
endif()

if(FILELINK_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks/dedup)
endif()
