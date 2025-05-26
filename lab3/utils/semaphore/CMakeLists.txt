add_library(semaphore STATIC
        src/semaphore.cpp
)

target_include_directories(semaphore PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(semaphore PUBLIC
        logger
        exceptions
)
