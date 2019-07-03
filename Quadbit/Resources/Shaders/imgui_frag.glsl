#version 460

layout(binding = 0) uniform sampler2D fontSampler;

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inColour;

layout(location = 0) out vec4 outColour;

void main() {
	outColour = inColour * texture(fontSampler, inUV);
}