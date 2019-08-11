#version 460

layout(binding = 0) uniform samplerCube samplerCubemap;

layout(location = 0) in vec3 inUVW;

layout(location = 0) out vec4 outColour;

void main() {
	outColour = texture(samplerCubemap, inUVW);
}