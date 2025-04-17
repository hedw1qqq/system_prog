cmake_minimum_required(VERSION 3.22)
project(lab2t1)

set(CMAKE_CXX_STANDARD 20)

include_directories(.)

add_library(my_lib src/ConsoleLogHandler.cpp
        src/FileLogHandler.cpp
        src/Logger.cpp)
target_include_directories(my_lib PUBLIC include)
add_executable(main main.cpp)
target_link_libraries(main PUBLIC my_lib)
