add_library(shared_memory STATIC
        src/shared_memory.cpp
)

target_include_directories(shared_memory PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(shared_memory PUBLIC
        logger
        exceptions
)
