#version 450

layout(binding = 1) uniform sampler2D shadowmap;

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inColour;
layout(location = 2) in vec3 inViewVec;
layout(location = 3) in vec3 inLightVec;
layout(location = 4) in vec3 inShadowCoord;


layout(location = 0) out vec4 outColour;

#define ambient 0.1

float textureProj(vec4 shadowCoord, vec2 offset) {
	float shadow = 1.0;
	if(shadowCoord.z > -1.0 && shadowCoord.z < 1.0) {
		float dist = texture(shadowmap, shadowCoord.st + offset).r;
		if(shadowCoord.w > 0.0 && dist < shadowCoord.z) {
			shadow = ambient;
		}
	}
	return shadow;
}

void main() {
	vec3 lightColour = vec3(1.0, 1.0, 1.0);

    float ambientStrength = 0.2;
    vec3 ambient = ambientStrength * lightColour;

    vec3 norm = normalize(inNormal);
    vec3 lightDirection = normalize(vec3(0.0, 1.0, 0.0));
    float diff = max(dot(norm, lightDirection), 0.0);
    vec3 diffuse = diff * lightColour;
    
    vec3 result = (ambient + diffuse) * inColour;
    outColour = vec4(result, 1.0);
}