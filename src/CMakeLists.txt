file(GLOB_RECURSE FILELINK_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)
# 从库源文件列表中排除 main.cpp，避免将入口函数打包进库
list(REMOVE_ITEM FILELINK_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")

file(GLOB_RECURSE FILELINK_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

# 核心库：包含所有业务逻辑
add_library(filelink_lib ${FILELINK_SOURCES} ${FILELINK_HEADERS})
target_include_directories(filelink_lib PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR} 
    ${soci_SOURCE_DIR}/include 
    ${soci_BINARY_DIR}/include
)
target_compile_features(filelink_lib PUBLIC cxx_std_14)
target_link_libraries(filelink_lib PUBLIC
    CLI11::CLI11
    nlohmann_json::nlohmann_json
    Tudou::tudou
    ${FILELINK_BLAKE3_TARGET}
)
target_link_libraries(filelink_lib PUBLIC
    PkgConfig::MySQLClient
    PkgConfig::Sodium
    soci_core
    soci_mysql
)

# 在线服务可执行程序
add_executable(filelink-server main.cpp)
target_link_libraries(filelink-server PRIVATE 
    filelink_lib
)
