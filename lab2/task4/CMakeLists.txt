cmake_minimum_required(VERSION 3.22)
project(task4)

set(CMAKE_CXX_STANDARD 20)


add_executable(elevator_sim
        main.cpp
        src/Logger.cpp
)