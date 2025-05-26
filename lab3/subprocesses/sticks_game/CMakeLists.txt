add_executable(sticks_game
        src/main.cpp
        src/sticks_game.cpp
)

target_include_directories(sticks_game PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/utils/message_queue/include
        ${CMAKE_SOURCE_DIR}/utils/logger/include
        ${CMAKE_SOURCE_DIR}/utils/exceptions/include
)

target_link_libraries(sticks_game PRIVATE
        message_queue
        logger
        exceptions
)

# install(TARGETS sticks_game RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)