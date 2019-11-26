#version 460

layout(binding = 0) uniform sampler2D baseColorMap;
layout(binding = 1) uniform sampler2D metallicRoughnessMap;
layout(binding = 2) uniform sampler2D normalMap;
layout(binding = 3) uniform sampler2D occlusionMap;
layout(binding = 4) uniform sampler2D emissiveMap;

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUV0;
layout(location = 2) in vec2 inUV1;

layout(location = 0) out vec4 outColour;

void main() {
	outColour = texture(baseColorMap, inUV0);
}