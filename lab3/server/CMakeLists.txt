add_executable(server
        src/main.cpp
        src/server.cpp
)

target_include_directories(server PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/utils/tcp/server/include
        ${CMAKE_SOURCE_DIR}/utils/shared_memory/include
        ${CMAKE_SOURCE_DIR}/utils/semaphore/include
        ${CMAKE_SOURCE_DIR}/utils/message_queue/include
        ${CMAKE_SOURCE_DIR}/utils/logger/include
        ${CMAKE_SOURCE_DIR}/utils/exceptions/include
        ${CMAKE_SOURCE_DIR}/subrocesses/sticks_game/include
        ${CMAKE_SOURCE_DIR}/subrocesses/compiler/include
)


target_link_libraries(server PUBLIC
        tcp_server
        shared_memory
        semaphore
        message_queue
        logger
        exceptions

)
find_package(Threads REQUIRED)
target_link_libraries(server PUBLIC
        Threads::Threads
        rt
)
