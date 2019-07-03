#version 460

layout(binding = 0, rgba32f) readonly uniform image2D displacement;

layout(push_constant) uniform PushConstants {
	mat4 model;
	mat4 MVP;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColour;

layout(location = 0) out vec3 outColour;
layout(location = 1) out float height;

void main() {
	vec4 D = imageLoad(displacement, ivec2(inPosition.xz));

	// Fix tiling
	if(inPosition.x == 512) {
		D = imageLoad(displacement, ivec2(0, inPosition.z)); 
	}
	if(inPosition.z == 512) {
		D = imageLoad(displacement, ivec2(inPosition.x, 0)); 
	}
	if(inPosition.x == 512 && inPosition.z == 512) {
		D = imageLoad(displacement, ivec2(0, 0));
	}

	// Choppy-ness factor
	float lambda = -0.5f;
	vec3 finalPos = vec3(inPosition.x + (D.x * lambda), inPosition.y + (D.y * 10.0f), inPosition.z + (D.z * lambda));

    gl_Position = pc.MVP * (vec4(finalPos, 1.0));
	outColour = inColour;
	height = D.y * 2.0f;
}