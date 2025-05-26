
add_executable(compiler
        src/main.cpp
        src/compiler.cpp
)

target_include_directories(compiler PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/utils/shared_memory/include
        ${CMAKE_SOURCE_DIR}/utils/semaphore/include
        ${CMAKE_SOURCE_DIR}/utils/logger/include
        ${CMAKE_SOURCE_DIR}/utils/exceptions/include
)

target_link_libraries(compiler PRIVATE
        shared_memory
        semaphore
        logger
        exceptions
)


set(SCRIPTS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/utils)
set(SCRIPTS_DEST_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

file(COPY ${SCRIPTS_SOURCE_DIR}/compile_cpp.sh DESTINATION ${SCRIPTS_DEST_DIR}
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
file(COPY ${SCRIPTS_SOURCE_DIR}/compile_tex.sh DESTINATION ${SCRIPTS_DEST_DIR}
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)


# install(FILES
#     ${SCRIPTS_SOURCE_DIR}/compile_cpp.sh
#     ${SCRIPTS_SOURCE_DIR}/compile_tex.sh
#     DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
#     PERMISSIONS OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE OWNER_READ GROUP_READ WORLD_READ
# )
# install(TARGETS compiler RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)