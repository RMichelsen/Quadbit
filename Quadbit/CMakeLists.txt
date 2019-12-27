cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Quadbit VERSION 0.1 LANGUAGES CXX)

find_package(Vulkan REQUIRED)

# For GLM
set(GLM_TEST_ENABLE OFF CACHE BOOL "" FORCE)
add_subdirectory(Dependencies/glm EXCLUDE_FROM_ALL)

add_subdirectory(Dependencies/imgui EXCLUDE_FROM_ALL)
add_subdirectory(Dependencies/EABase EXCLUDE_FROM_ALL)
add_subdirectory(Dependencies/EASTL EXCLUDE_FROM_ALL)
add_subdirectory(Dependencies/SPIRV-Cross EXCLUDE_FROM_ALL)

# For glslang
set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(SKIP_GLSLANG_INSTALL ON CACHE BOOL "" FORCE)
set(ENABLE_CTEST OFF CACHE BOOL "" FORCE)
set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(ENABLE_PCH OFF CACHE BOOL "" FORCE)
set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
add_subdirectory(Dependencies/glslang EXCLUDE_FROM_ALL)

add_custom_target(glmNatVis SOURCES Dependencies/glm/util/glm.natvis)
add_custom_target(EASTLNatVis SOURCES Dependencies/EASTL/doc/EASTL.natvis)
    
set(QUADBIT_SOURCES
   Source/Engine/API/Compute.h
   Source/Engine/API/Compute.cpp
   Source/Engine/API/Graphics.h
   Source/Engine/API/Graphics.cpp

   Source/Engine/Application/Window.h
   Source/Engine/Application/Window.cpp
   Source/Engine/Application/InputHandler.h
   Source/Engine/Application/InputHandler.cpp
   Source/Engine/Application/InputTypes.h

   Source/Engine/Core/Entry.h
   Source/Engine/Core/Entry.cpp
   Source/Engine/Core/Game.h
   Source/Engine/Core/Logging.h
   Source/Engine/Core/Sfinae.h
   Source/Engine/Core/Time.h

   Source/Engine/Entities/EntityManager.h
   Source/Engine/Entities/EntityManager.cpp
   Source/Engine/Entities/EntityTypes.h
   Source/Engine/Entities/SparseSet.h
   Source/Engine/Entities/SystemDispatch.h

   Source/Engine/Rendering/Renderer.h
   Source/Engine/Rendering/Renderer.cpp
   Source/Engine/Rendering/RenderTypes.h
   Source/Engine/Rendering/VulkanTypes.h
   Source/Engine/Rendering/VulkanUtils.h

   Source/Engine/Rendering/Geometry/Icosphere.h
   Source/Engine/Rendering/Geometry/Plane.h

   Source/Engine/Rendering/Memory/Allocator.h
   Source/Engine/Rendering/Memory/Allocator.cpp
   Source/Engine/Rendering/Memory/Pool.h
   Source/Engine/Rendering/Memory/Pool.cpp
   Source/Engine/Rendering/Memory/ResourceManager.h
   Source/Engine/Rendering/Memory/ResourceManager.cpp

   Source/Engine/Rendering/Pipelines/Pipeline.h
   Source/Engine/Rendering/Pipelines/Pipeline.cpp
   Source/Engine/Rendering/Pipelines/PipelinePresets.h
   Source/Engine/Rendering/Pipelines/ImGuiPipeline.h
   Source/Engine/Rendering/Pipelines/ImGuiPipeline.cpp
   Source/Engine/Rendering/Pipelines/PBRPipeline.h
   Source/Engine/Rendering/Pipelines/PBRPipeline.cpp
   Source/Engine/Rendering/Pipelines/SkyPipeline.h
   Source/Engine/Rendering/Pipelines/SkyPipeline.cpp

   Source/Engine/Rendering/Shaders/ShaderCompiler.h
   Source/Engine/Rendering/Shaders/ShaderCompiler.cpp
   Source/Engine/Rendering/Shaders/ShaderInstance.h
   Source/Engine/Rendering/Shaders/ShaderInstance.cpp

   Source/Engine/Rendering/Systems/NoClipCameraSystem.h
)

add_library(Quadbit STATIC ${QUADBIT_SOURCES})
source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${QUADBIT_SOURCES})

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
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Source>
		EASTL
    PRIVATE
        Dependencies/tinygltf/include
        Dependencies/stb/include
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