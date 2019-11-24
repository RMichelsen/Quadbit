cmake_minimum_required(VERSION 3.14)

project(SPIRV-Cross VERSION 1.0 LANGUAGES C CXX)

add_library(SPIRV-Cross STATIC)

target_sources(SPIRV-Cross
    PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/SPIRV-Cross/spirv_cross.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/SPIRV-Cross/spirv_parser.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/SPIRV-Cross/spirv_cross_parsed_ir.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/SPIRV-Cross/spirv_cfg.cpp
    )

target_include_directories(SPIRV-Cross INTERFACE ${CMAKE_CURRENT_LIST_DIR})