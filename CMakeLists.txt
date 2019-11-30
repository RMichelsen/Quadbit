cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Quadbit_Projects)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Build/Out)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Build/Out)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Build/Out)

if (MSVC)
    # add_compile_options(/GR-) # Disable RTTI
    add_compile_options(/EHsc)
else()
    # add_compile_options(-fno-rtti) # Disable RTTI
    add_compile_options(-fno-exceptions)
endif()

# Add library
add_subdirectory(quadbit)

# Add examples
add_subdirectory(examples/Voxels)
add_subdirectory(examples/Water)
add_subdirectory(examples/Testing)

set_target_properties(Voxels PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_target_properties(Water PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_target_properties(Testing PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

set_target_properties(
    Voxels
    Water
    Testing  
        PROPERTIES FOLDER Examples
    )

set_target_properties(
    glslang 
    OGLCompiler 
    OSDependent 
    SPIRV 
    EASTL
    EASTLNatVis
    FastNoiseSIMD
    glmNatVis
    ImGui
    SPIRV-Cross    
        PROPERTIES FOLDER Dependencies
    )