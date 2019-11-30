#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(binding = 0, rgba32f) uniform readonly image2D Dx;
layout(binding = 1, rgba32f) uniform readonly image2D Dy;
layout(binding = 2, rgba32f) uniform readonly image2D Dz;
layout(binding = 3, rgba32f) uniform readonly image2D D_slopex;
layout(binding = 4, rgba32f) uniform readonly image2D D_slopez;
layout(binding = 5, rgba32f) uniform writeonly image2D displacement_map;
layout(binding = 6, rgba32f) uniform writeonly image2D normal_map;

void main() {
	// Choppy-ness factor
	float lambda = 1.5f;

	// Assemble the RGB displacement map from the individual displacement components
	vec4 DxPixel = imageLoad(Dx, ivec2(gl_GlobalInvocationID.xy));
	vec4 DyPixel = imageLoad(Dy, ivec2(gl_GlobalInvocationID.xy));
	vec4 DzPixel = imageLoad(Dz, ivec2(gl_GlobalInvocationID.xy));
	imageStore(displacement_map, ivec2(gl_GlobalInvocationID.xy), vec4(DxPixel.x * lambda, DyPixel.x, DzPixel.x * lambda, 0.0f));

	// Assemble the normal map 
	vec4 D_slopex_val = imageLoad(D_slopex, ivec2(gl_GlobalInvocationID.xy));
	vec4 D_slopez_val = imageLoad(D_slopez, ivec2(gl_GlobalInvocationID.xy));
	vec3 N = normalize(vec3(-D_slopex_val.x, 1.0f * 3.5f, -D_slopez_val.x));

	imageStore(normal_map, ivec2(gl_GlobalInvocationID.xy), vec4(N, 0.0f));
}