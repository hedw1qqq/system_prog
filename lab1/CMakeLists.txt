cmake_minimum_required(VERSION 3.21)
project(lab1 C)

set(CMAKE_C_STANDARD 11)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -pedantic -fsanitize=address -fsanitize=leak")
add_executable(task7
        "task7/main.c")
add_executable(task3
        "task3/main.c")

add_executable(task1
        "task1/main.c"
        task1/functions.c
        task1/functions.h)



add_executable(task6 "task6/server.c"
        "task6/client.c"
        "task6/common.h"
)
add_executable(task5 task5/main.c)

add_executable(task2 task2/main.c)

add_executable(task4 "task4/server.c"
        "task4/client.c"
        "task4/common.h"
)