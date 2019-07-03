#version 450

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(binding = 0, rgba32f) uniform readonly image2D Dx;
layout(binding = 1, rgba32f) uniform readonly image2D Dy;
layout(binding = 2, rgba32f) uniform readonly image2D Dz;
layout(binding = 3, rgba32f) uniform writeonly image2D displacement_map;

void main() {
	vec4 DxPixel = imageLoad(Dx, ivec2(gl_GlobalInvocationID.xy));
	vec4 DyPixel = imageLoad(Dy, ivec2(gl_GlobalInvocationID.xy));
	vec4 DzPixel = imageLoad(Dz, ivec2(gl_GlobalInvocationID.xy));
	imageStore(displacement_map, ivec2(gl_GlobalInvocationID.xy), vec4(DxPixel.x, DyPixel.x, DzPixel.x, 0.0f));
}