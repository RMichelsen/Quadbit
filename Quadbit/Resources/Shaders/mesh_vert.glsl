#version 450

layout(binding = 0) uniform UniformBufferObject {
	mat4 lightSpace;
	vec3 lightPosition;
} ubo;

layout(push_constant) uniform PushConstants {
	mat4 model;
	mat4 MVP;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColour;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outColour;
layout(location = 2) out vec3 outViewVec;
layout(location = 3) out vec3 outLightVec;
layout(location = 4) out vec4 outShadowCoord;

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 
);

void main() {
    gl_Position = pc.MVP * vec4(inPosition, 1.0);

	vec4 worldPos = pc.model * vec4(inPosition, 1.0);
	outNormal = mat3(transpose(inverse(pc.model))) * inNormal;
	outColour = inColour;
	outViewVec = -worldPos.xyz;
	outLightVec = normalize(ubo.lightPosition - inPosition);
	outShadowCoord = (biasMat * ubo.lightSpace * pc.model) * vec4(inPosition, 1.0);
}