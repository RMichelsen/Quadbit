cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Testing LANGUAGES CXX)

add_executable(Testing WIN32
    src/Main.cpp
    src/Game.cpp
    )

target_link_libraries(Testing
    PRIVATE
        Quadbit
    )

#add_custom_command(TARGET Testing PRE_BUILD
#                   COMMAND ${CMAKE_COMMAND} -E copy_directory
#                       ${CMAKE_CURRENT_SOURCE_DIR}/resources/ $<TARGET_FILE_DIR:Testing>/resources)