add_executable(filelink-dedup-tests
    Blake3HashTests.cpp
    DuplicateFinderTests.cpp
    FileWalkerTests.cpp
    HasherTests.cpp
)

target_link_libraries(filelink-dedup-tests PRIVATE
    FileLink::dedup
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(filelink-dedup-tests)

if(FILELINK_BUILD_DEDUP_TOOL)
    find_program(BASH_EXECUTABLE bash REQUIRED)
    add_test(
        NAME filelinkDedupCliTests
        COMMAND ${BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_cli_tests.sh $<TARGET_FILE:filelink-dedup>
    )
endif()
