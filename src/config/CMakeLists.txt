add_library(filelink_config
    AppConfig.cpp
)
add_library(FileLink::config ALIAS filelink_config)

target_include_directories(filelink_config PUBLIC
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(filelink_config PRIVATE
    CLI11::CLI11
)
