cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Water LANGUAGES CXX)

add_executable(Water WIN32
    src/Water.cpp
    )

target_link_libraries(Water
    PRIVATE
        Quadbit
    )

add_custom_command(TARGET Water PRE_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_CURRENT_SOURCE_DIR}/resources/ $<TARGET_FILE_DIR:Water>/resources)