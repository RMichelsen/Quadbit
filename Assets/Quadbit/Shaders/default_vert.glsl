#version 460

layout(push_constant) uniform PushConstants {
	mat4 model;
	mat4 MVP;
} pc;

layout(binding = 0) uniform SunUBO {
	float azimuth;
	float altitude;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV0;
layout(location = 3) in vec2 inUV1;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outUV0;
layout(location = 2) out vec2 outUV1;
layout(location = 3) out vec3 outViewVec;
layout(location = 4) out vec3 outSunViewVec;

void main() {
    gl_Position = pc.MVP * vec4(inPosition, 1.0);
	outNormal = inNormal;
	outUV0 = inUV0;
	outUV1 = inUV1;
	outSunViewVec = vec3(sin(ubo.altitude) * cos(ubo.azimuth), cos(ubo.altitude), sin(ubo.altitude) * sin(ubo.azimuth));
	outViewVec = -(pc.model * vec4(inPosition, 1.0)).xyz;
}