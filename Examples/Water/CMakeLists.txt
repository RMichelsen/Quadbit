cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Water LANGUAGES CXX)

set(WATER_SOURCES
    Source/Water.h
    Source/Water.cpp
)

source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${WATER_SOURCES})

add_executable(Water WIN32 ${WATER_SOURCES})

target_link_libraries(Water
    PRIVATE
        Quadbit
    )