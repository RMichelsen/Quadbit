#version 460

layout(push_constant) uniform PushConstants {
	mat4 proj;
	mat4 view;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColour;

layout(location = 0) out vec4 outColour;


void main() {
	gl_Position = pc.proj * pc.view * vec4(inPosition, 1.0);
	outColour = vec4(inColour, 1.0);
}