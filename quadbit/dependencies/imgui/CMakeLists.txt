cmake_minimum_required(VERSION 3.14)

project(ImGui VERSION 1.0 LANGUAGES C CXX)

add_library(ImGui STATIC)

target_sources(ImGui
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_demo.cpp
        ${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_draw.cpp
        ${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_widgets.cpp
        ${CMAKE_CURRENT_LIST_DIR}/imgui/imgui.cpp
    )

target_include_directories(ImGui INTERFACE ${CMAKE_CURRENT_LIST_DIR})