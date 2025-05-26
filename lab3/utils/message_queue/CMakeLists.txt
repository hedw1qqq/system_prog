add_library(message_queue STATIC
        src/client_message_queue.cpp
        src/server_message_queue.cpp
)

target_include_directories(message_queue PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(message_queue PUBLIC
        logger
        exceptions
)
