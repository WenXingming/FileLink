add_executable(filelink-storage-tests
    LocalObjectStoreTests.cpp
)

target_link_libraries(filelink-storage-tests PRIVATE
    FileLink::storage
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(filelink-storage-tests)
