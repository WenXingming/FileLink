add_executable(filelink-server
    main.cpp
)

target_link_libraries(filelink-server PRIVATE
    FileLink::config
    Tudou::tudou
)
