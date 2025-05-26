
add_library(client_lib STATIC
        src/client.cpp
)
target_include_directories(client_lib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/utils/tcp/client/include
        ${CMAKE_SOURCE_DIR}/utils/logger/include
        ${CMAKE_SOURCE_DIR}/utils/exceptions/include
)
target_link_libraries(client_lib PUBLIC
        tcp_client
        logger
        exceptions
)

add_executable(client
        src/main.cpp
)
target_include_directories(client PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(client PUBLIC
        client_lib
        Threads::Threads
)

find_package(Threads REQUIRED)
