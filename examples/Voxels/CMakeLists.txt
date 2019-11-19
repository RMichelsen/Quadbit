cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Voxels LANGUAGES CXX)

add_subdirectory(src/Extern/FastNoiseSIMD)

add_executable(Voxels WIN32
    src/Infinitum.cpp
    )

target_include_directories(Voxels
    PRIVATE
        src/Extern
    )

target_link_libraries(Voxels
    PRIVATE
        Quadbit
        FastNoiseSIMD
    )

add_custom_command(TARGET Voxels PRE_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_CURRENT_SOURCE_DIR}/resources/ $<TARGET_FILE_DIR:Voxels>/resources)