# Quadbit
Quadbit is a Vulkan game engine written in C++

# Current Feature-Set
- Multi sampling
- Model and texture loading
- Compute pipeline
- Pipeline for visual debugging
- Data-Oriented Entity Component System (ECS)

# Planned features
- Shadow mapping
- Instancing and indirect drawing
- Occlusion culling
- Nvidia Raytracing (RTX)

# Examples
## Runtime water generation using inverse Fourier transforms in compute shaders
![](Media/Water.gif)

## Voxel generation with ambient occlusion and PhysX
![](Media/Voxels_AO.png)

# Libraries and Dependencies
- [Vulkan](https://www.khronos.org/vulkan/)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [stb_image](https://github.com/nothings/stb)
- [OpenGL Image (GLI)](https://github.com/g-truc/gli)
- [OpenGL Mathematics (GLM)](https://github.com/g-truc/glm)
- [tinyobjloader](https://github.com/syoyo/tinyobjloader)