cmake_minimum_required(VERSION 3.14)

project(FastNoiseSIMD VERSION 1.0 LANGUAGES C CXX)

add_library(FastNoiseSIMD STATIC)

target_sources(FastNoiseSIMD
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/FastNoiseSIMD/FastNoiseSIMD.cpp
        ${CMAKE_CURRENT_LIST_DIR}/FastNoiseSIMD/FastNoiseSIMD_avx2.cpp
        ${CMAKE_CURRENT_LIST_DIR}/FastNoiseSIMD/FastNoiseSIMD_avx512.cpp
        ${CMAKE_CURRENT_LIST_DIR}/FastNoiseSIMD/FastNoiseSIMD_internal.cpp
        ${CMAKE_CURRENT_LIST_DIR}/FastNoiseSIMD/FastNoiseSIMD_neon.cpp
        ${CMAKE_CURRENT_LIST_DIR}/FastNoiseSIMD/FastNoiseSIMD_sse2.cpp
        ${CMAKE_CURRENT_LIST_DIR}/FastNoiseSIMD/FastNoiseSIMD_sse41.cpp
    )

target_include_directories(FastNoiseSIMD INTERFACE ${CMAKE_CURRENT_LIST_DIR})