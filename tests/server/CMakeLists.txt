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
