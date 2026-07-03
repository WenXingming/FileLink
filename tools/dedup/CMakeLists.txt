add_executable(filelink-dedup main.cpp)

target_link_libraries(filelink-dedup
    PRIVATE
        FileLink::dedup
        CLI11::CLI11
)
