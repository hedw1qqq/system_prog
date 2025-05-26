add_library(tcp_client STATIC
        src/TCPClient.cpp
)

target_include_directories(tcp_client PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tcp_client PUBLIC
        logger
        exceptions
)
