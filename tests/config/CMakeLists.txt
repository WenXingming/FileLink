add_executable(filelink-config-tests
    AppConfigTests.cpp
)

target_link_libraries(filelink-config-tests PRIVATE
    FileLink::config
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(filelink-config-tests)
