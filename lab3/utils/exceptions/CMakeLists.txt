add_library(exceptions STATIC
        src/custom_exceptions.cpp
)

target_include_directories(exceptions PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
