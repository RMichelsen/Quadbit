#version 460

layout(binding = 1) uniform sampler2D normal_map;

layout(binding = 2) uniform UBO {
	vec4 topColour;
	vec4 botColour;
	vec3 cameraPos;
	int useNormalMap;
	float colourIntensity;
} ubo;

layout(binding = 3) uniform samplerCube environment_map;

layout(location = 0) in vec3 inColour;
layout(location = 1) in vec2 inTextPos;
layout(location = 2) in vec3 inFragPos;
layout(location = 3) in float inHeight;

layout(location = 0) out vec4 outColour;

void main() {
	int imageExtents = textureSize(normal_map, 0).x;

	vec4 baseColour = mix(ubo.topColour, ubo.botColour, inHeight);

	vec4 N;
	if(ubo.useNormalMap == 1) N = texture(normal_map, inTextPos * (1.0f / imageExtents));
	else N = vec4(0, 1, 0, 0);

	float air_ior = 1.0f;
	float water_ior = 1.33f;
	vec3 I = normalize(inFragPos - ubo.cameraPos);

	vec3 Refl = reflect(I, N.xyz);
	vec3 Refr = refract(I, N.xyz, air_ior / water_ior);

	// Shlick approximation
	float R_0 = ((air_ior - water_ior) / (air_ior + water_ior)) * ((air_ior - water_ior) / (air_ior + water_ior));

	float cos_theta = -dot(I, N.xyz);
	float R_theta = R_0 + (1 - R_0) * (1 - cos_theta) * (1 - cos_theta) * (1 - cos_theta) * (1 - cos_theta) * (1 - cos_theta);

	//outColour = diff * baseColour;
	vec4 refractionColour = mix(vec4(texture(environment_map, Refr).rgb, 1.0f), baseColour, ubo.colourIntensity);
	vec4 reflectionColour = vec4(texture(environment_map, Refl).rgb, 1.0f);
	outColour = mix(refractionColour, reflectionColour, R_theta);

	outColour = baseColour;
}