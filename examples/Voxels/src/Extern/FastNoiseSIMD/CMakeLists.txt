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

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/FastNoiseSIMD/FastNoiseSIMD_avx2.cpp PROPERTIES COMPILE_FLAGS /arch:AVX2)
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/FastNoiseSIMD/FastNoiseSIMD_avx512.cpp PROPERTIES COMPILE_FLAGS /arch:AVX2)
else()
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/FastNoiseSIMD/FastNoiseSIMD_avx2.cpp PROPERTIES COMPILE_FLAGS -mavx2)
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/FastNoiseSIMD/FastNoiseSIMD_avx512.cpp PROPERTIES COMPILE_FLAGS -mavx512f)
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/FastNoiseSIMD/FastNoiseSIMD_sse2.cpp PROPERTIES COMPILE_FLAGS -msse2)
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/FastNoiseSIMD/FastNoiseSIMD_sse41.cpp PROPERTIES COMPILE_FLAGS -msse4.1)
endif()

target_include_directories(FastNoiseSIMD INTERFACE ${CMAKE_CURRENT_LIST_DIR})