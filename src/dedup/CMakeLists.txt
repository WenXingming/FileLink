add_library(filelink_dedup
    DuplicateFinder.cpp
    FileWalker.cpp
    Hasher.cpp
)

add_library(FileLink::dedup ALIAS filelink_dedup)

target_include_directories(filelink_dedup
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_compile_features(filelink_dedup PUBLIC cxx_std_14)

target_link_libraries(filelink_dedup
    PUBLIC
        ThreadPool
        ${FILELINK_BLAKE3_TARGET}
)

if(FILELINK_BUILD_DEDUP_TOOL)
    add_executable(filelink-dedup main.cpp)

    target_link_libraries(filelink-dedup
        PRIVATE
            FileLink::dedup
            CLI11::CLI11
    )
endif()
