add_library(logger STATIC
        src/logger.cpp
)

target_include_directories(logger PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

