add_library(tcp_server STATIC
        src/TCPServer.cpp
)

target_include_directories(tcp_server PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tcp_server PUBLIC
        logger
        exceptions
)
