#version 460

layout(binding = 1) uniform sampler2D normal_map;

layout(location = 0) in vec3 inColour;
layout(location = 1) in vec2 inTextPos;
layout(location = 2) in vec3 inFragPos;
layout(location = 3) in float inHeight;

layout(location = 0) out vec4 outColour;

void main() {
	vec4 lightBlue = vec4(0.15f, 0.7f, 0.8f, 0.0f);
	vec4 darkBlue = vec4(0.10f, 0.45f, 0.7f, 0.0f);

	vec4 baseColour = mix(lightBlue, darkBlue, inHeight);

	vec4 N = texture(normal_map, inTextPos * (1.0f / 1024.0f));

	float diff = max(dot(N.xyz, normalize(vec3(0, 1000, 1024) - inFragPos)), 0.0);

	outColour = diff * baseColour;
}