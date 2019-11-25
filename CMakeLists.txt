cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Quadbit_Projects)

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