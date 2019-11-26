cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Testing LANGUAGES CXX)

set(TESTING_SOURCES
    Source/Testing.h
    Source/Testing.cpp
)

source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${TESTING_SOURCES})

add_executable(Testing WIN32 ${TESTING_SOURCES})

target_link_libraries(Testing
    PRIVATE
        Quadbit
    )