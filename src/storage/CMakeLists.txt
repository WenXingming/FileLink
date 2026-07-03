add_library(filelink_storage
    LocalObjectStore.cpp
)
add_library(FileLink::storage ALIAS filelink_storage)

target_include_directories(filelink_storage PUBLIC
    ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(filelink_storage PUBLIC cxx_std_14)
