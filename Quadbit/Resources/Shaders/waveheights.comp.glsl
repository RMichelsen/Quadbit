#version 450

layout (local_size_x = 16, local_size_y = 16) in;

layout(binding = 0) uniform UBO {
	float time;
	int N;
	vec2 W;
	vec2 WN;
	float A;
	int L;
} ubo;
layout(binding = 1, rgba8) uniform image2D h0s;
layout(binding = 2, rgba8) uniform image2D h0s_conj;

void main() {
	imageStore(h0s, ivec2(gl_GlobalInvocationID.xy), vec4(0.2f, 0.5f, 0.7f, 1.0f));
	imageStore(h0s_conj, ivec2(gl_GlobalInvocationID.xy), vec4(0.2f, 0.7f, 0.5f, 1.0f));
}