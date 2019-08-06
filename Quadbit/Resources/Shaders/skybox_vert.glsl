#version 460

layout(push_constant) uniform PushConstants {
	mat4 proj;
	mat4 view;
} pc;

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 outUVW;

void main() {
	outUVW = inPosition;
	gl_Position = pc.proj * pc.view * vec4(inPosition, 1.0);
}