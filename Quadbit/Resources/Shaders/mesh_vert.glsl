#version 460

layout(push_constant) uniform PushConstants {
	mat4 model;
	mat4 MVP;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColour;

layout(location = 0) out vec3 outColour;

void main() {
    gl_Position = pc.MVP * vec4(inPosition, 1.0);
	outColour = inColour;
}