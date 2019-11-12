cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Voxels LANGUAGES CXX)

find_package(Vulkan REQUIRED)

add_subdirectory(Source/Extern/FastNoiseSIMD)

add_executable(Voxels WIN32
    Source/Main.cpp
    Source/Infinitum.cpp
    )

target_include_directories(Voxels
    PRIVATE
        Source/Extern
    )

target_link_libraries(Voxels
    PRIVATE
        Quadbit
        FastNoiseSIMD
    )

add_custom_command(TARGET Voxels PRE_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_CURRENT_SOURCE_DIR}/Resources/ $<TARGET_FILE_DIR:Voxels>/Resources)