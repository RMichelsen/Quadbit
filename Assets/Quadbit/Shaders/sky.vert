#version 460

layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform PushConstants {
	mat4 MVP;
	float azimuth;
	float altitude;
} pc;

layout(location = 0) out vec3 outViewVec;
layout(location = 1) out vec3 outSunViewVec;

void main() {
	outViewVec = -(vec4(inPosition, 1.0)).xyz;
	outSunViewVec = vec3(sin(pc.altitude) * cos(pc.azimuth), cos(pc.altitude), sin(pc.altitude) * sin(pc.azimuth));

    gl_Position = pc.MVP * vec4(inPosition, 1.0);
}