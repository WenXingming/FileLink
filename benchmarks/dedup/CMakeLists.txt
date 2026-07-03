# dedup 性能基准 target

include(FetchContent)

find_package(benchmark QUIET)
if(NOT benchmark_FOUND)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        googlebenchmark
        URL https://github.com/google/benchmark/archive/refs/tags/v1.9.5.tar.gz
    )
    FetchContent_MakeAvailable(googlebenchmark)
endif()

add_executable(filelink-dedup-benchmark
    DedupBenchmark.cpp
)

target_compile_features(filelink-dedup-benchmark PRIVATE cxx_std_17)

target_link_libraries(filelink-dedup-benchmark PRIVATE
    FileLink::dedup
    benchmark::benchmark
)

add_custom_target(compare_jdupes
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compare_jdupes.sh $<TARGET_FILE:filelink-dedup>
    DEPENDS filelink-dedup
    COMMENT "运行 FileLink dedup 与 jdupes 的系统级性能对比"
)
