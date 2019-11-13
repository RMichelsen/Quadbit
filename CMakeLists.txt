cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Quadbit_Projects)

# Add library
add_subdirectory(quadbit)

# Add examples
add_subdirectory(examples/Voxels)
add_subdirectory(examples/Water)
add_subdirectory(examples/Testing)