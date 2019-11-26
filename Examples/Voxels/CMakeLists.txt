cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Voxels LANGUAGES CXX)

add_subdirectory(Source/Extern/FastNoiseSIMD EXCLUDE_FROM_ALL)

set(VOXEL_SOURCES
    Source/Infinitum.h
    Source/Infinitum.cpp

    Source/Data/Components.h
    Source/Data/Defines.h

    Source/Systems/MeshGenerationSystem.h
    Source/Systems/PlayerMovementSystem.h
    Source/Systems/ThirdPersonCameraSystem.h
    Source/Systems/VoxelGenerationSystem.h

    Source/Utils/CommonUtils.h
    Source/Utils/MagicaVoxImporter.h
    Source/Utils/TerrainTools.h
    Source/Utils/VoxelMesher.h
)

source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${VOXEL_SOURCES})

add_executable(Voxels WIN32 ${VOXEL_SOURCES})

target_include_directories(Voxels
    PRIVATE
        Source/Extern
    )

target_link_libraries(Voxels
    PRIVATE
        Quadbit
        FastNoiseSIMD
    )