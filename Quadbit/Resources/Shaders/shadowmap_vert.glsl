#version 450

layout (binding = 0) uniform UBO {
	mat4 depthMVP;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColour;

void main() {
	gl_Position =  ubo.depthMVP * vec4(inPosition, 1.0);
}