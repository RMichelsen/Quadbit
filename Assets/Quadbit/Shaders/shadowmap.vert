#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV0;
layout(location = 3) in vec2 inUV1;

layout(push_constant) uniform PushConstants {
	mat4 depthMVP;
} pc;

void main() {
    gl_Position = pc.depthMVP * vec4(inPosition, 1.0);
}