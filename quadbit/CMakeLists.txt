cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Quadbit VERSION 0.1 LANGUAGES CXX)

find_package(Vulkan REQUIRED)

set(GLM_TEST_ENABLE OFF CACHE BOOL "" FORCE)

add_subdirectory(dependencies/glm)
add_subdirectory(dependencies/imgui)
add_subdirectory(dependencies/EABase)
add_subdirectory(dependencies/EASTL)
add_subdirectory(dependencies/SPIRV-Cross)

# For glslang
set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(SKIP_GLSLANG_INSTALL ON CACHE BOOL "" FORCE)
set(ENABLE_CTEST OFF CACHE BOOL "" FORCE)
set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(ENABLE_PCH OFF CACHE BOOL "" FORCE)
set(ENABLE_OPT OFF CACHE BOOL "" FORCE)

add_subdirectory(dependencies/glslang)

add_custom_target(glmNatVis SOURCES dependencies/glm/util/glm.natvis)
add_custom_target(EASTLNatVis SOURCES dependencies/EASTL/doc/EASTL.natvis)

add_library(Quadbit STATIC
    src/Engine/API/Compute.cpp
    src/Engine/API/Graphics.cpp
    src/Engine/Application/Window.cpp
    src/Engine/Application/InputHandler.cpp
    src/Engine/Core/Entry.cpp
    src/Engine/Entities/EntityManager.cpp
    src/Engine/Rendering/Renderer.cpp
    src/Engine/Rendering/Shaders/ShaderInstance.cpp
    src/Engine/Rendering/Shaders/ShaderCompiler.cpp
    src/Engine/Rendering/Memory/Allocator.cpp
    src/Engine/Rendering/Memory/Pool.cpp
    src/Engine/Rendering/Memory/ResourceManager.cpp
    src/Engine/Rendering/Pipelines/Pipeline.cpp
    src/Engine/Rendering/Pipelines/ImGuiPipeline.cpp
    src/Engine/Rendering/Pipelines/PBRPipeline.cpp
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
		EASTL
    PRIVATE
        dependencies/tinygltf/include
        dependencies/stb/include
        SPIRV-Cross
        glslang
    )

target_link_libraries(Quadbit
    PUBLIC
        glm
        ImGui
        Vulkan::Vulkan
		EASTL
        SPIRV-Cross
        glslang
        SPIRV
    )