#version 460

layout(binding = 0, rgba32f) readonly uniform image2D displacement_map;

layout(push_constant) uniform PushConstants {
	mat4 model;
	mat4 MVP;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColour;

layout(location = 0) out vec3 outColour;
layout(location = 1) out vec2 outTexPos;
layout(location = 2) out vec3 outFragPos;
layout(location = 3) out float outHeight;

void main() {
	outColour = inColour;
	outTexPos = inPosition.xz;

	vec4 D = imageLoad(displacement_map, ivec2(inPosition.xz));

	// Fix tiling
	if(inPosition.x == 1024) {
		D = imageLoad(displacement_map, ivec2(0, inPosition.z)); 
	}
	if(inPosition.z == 1024) {
		D = imageLoad(displacement_map, ivec2(inPosition.x, 0)); 
	}
	if(inPosition.x == 1024 && inPosition.z == 1024) {
		D = imageLoad(displacement_map, ivec2(0, 0));
	}

	vec3 absPos = vec3(inPosition.x + D.x, inPosition.y + D.y, inPosition.z + D.z);

    gl_Position = pc.MVP * vec4(absPos, 1.0);

	outFragPos = vec3(pc.model * vec4(absPos, 1.0));
	outHeight = D.y / 5.0f;
}