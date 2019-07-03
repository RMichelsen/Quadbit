#version 460

layout(location = 0) in vec3 inColour;
layout(location = 1) in float height;

layout(location = 0) out vec4 outColour;

void main() {
	vec4 lightBlue = vec4(0.25f, 0.4f, 0.88f, 0.0f);
	vec4 darkBlue = vec4(0.0f, 0.1f, 0.5f, 0.0f);

	outColour = mix(lightBlue, darkBlue, height);
}