#version 460

layout(binding = 0) uniform sampler2D textureSampler;

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec4 outColour;

void main() {
	outColour = texture(textureSampler, inUV);
}