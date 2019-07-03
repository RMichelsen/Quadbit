#version 450

layout(binding = 0, rgba32f) readonly uniform image2D h0_tilde_ty;

layout(location = 0) in vec3 inColour;
layout(location = 1) in vec2 inImagePos;

layout(location = 0) out vec4 outColour;

void main() {
	vec4 c = imageLoad(h0_tilde_ty, ivec2(inImagePos));
	outColour = c;
}