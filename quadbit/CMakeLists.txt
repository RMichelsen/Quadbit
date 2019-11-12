cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Quadbit VERSION 0.1 LANGUAGES CXX)

find_package(Vulkan REQUIRED)

set(GLM_TEST_ENABLE OFF CACHE BOOL "" FORCE)
add_subdirectory(dependencies/glm)
add_subdirectory(dependencies/imgui)
add_subdirectory(dependencies/tinyobjloader)

add_library(Quadbit STATIC
    src/Engine/Application/Window.cpp
    src/Engine/Entities/EntityManager.cpp
    src/Engine/Rendering/QbVkRenderer.cpp
    src/Engine/Rendering/QbVkShaderInstance.cpp
    src/Engine/Rendering/Memory/QbVkAllocator.cpp
    src/Engine/Rendering/Memory/QbVkPool.cpp
    src/Engine/Rendering/Pipelines/ComputePipeline.cpp
    src/Engine/Rendering/Pipelines/ImGuiPipeline.cpp
    src/Engine/Rendering/Pipelines/MeshPipeline.cpp
    )

target_compile_definitions(Quadbit 
    PRIVATE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        VK_USE_PLATFORM_WIN32_KHR
        GLM_FORCE_RADIANS
        GLM_FORCE_DEPTH_ZERO_TO_ONE
    )

target_include_directories(Quadbit
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    PRIVATE
        dependencies/stb
        Vulkan::Vulkan
    )

target_link_libraries(Quadbit
    PUBLIC
        glm
        ImGui
        tinyobjloader
        Vulkan::Vulkan
    )