#version 460

layout(binding = 1) uniform sampler2D shadowMap;
layout(binding = 2) uniform sampler2D baseColorMap;
layout(binding = 3) uniform sampler2D metallicRoughnessMap;
layout(binding = 4) uniform sampler2D normalMap;
layout(binding = 5) uniform sampler2D occlusionMap;
layout(binding = 6) uniform sampler2D emissiveMap;

layout(binding = 7) uniform MaterialUBO {
	vec4 baseColorFactor;
	vec4 emissiveFactor;
	float metallicFactor;
	float roughnessFactor;
	float alphaMask;
	float alphaMaskCutoff;
	int baseColorTextureIndex;
	int metallicRoughnessTextureIndex;
	int normalTextureIndex;
	int occlusionTextureIndex;
	int emissiveTextureIndex;
} ubo;

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUV0;
layout(location = 2) in vec2 inUV1;
layout(location = 3) in vec3 inViewVec;
layout(location = 4) in vec3 inSunViewVec;

layout(location = 0) out vec4 outColour;

vec4 LinearFromSRGB(vec4 srgb) {
	return vec4(pow(srgb.xyz, vec3(2.2)), srgb.w);
}

void main() {
	vec4 baseColour = (ubo.baseColorTextureIndex > -1) ?
		LinearFromSRGB(texture(baseColorMap, ubo.baseColorTextureIndex == 0 ? inUV0 : inUV1)) : 
		ubo.baseColorFactor;

	if(ubo.alphaMask == 1.0f) {
		if(baseColour.a < ubo.alphaMaskCutoff) {
			discard;
		}
		else {
			baseColour *= ubo.baseColorFactor;
		}
	}

	if(ubo.emissiveTextureIndex > -1) {
		vec4 emissive = LinearFromSRGB(texture(emissiveMap, ubo.emissiveTextureIndex == 0 ? inUV0 : inUV1)) * ubo.emissiveFactor;
		baseColour += emissive;
	}

	vec3 N = normalize(inNormal);
	vec3 L = normalize(-inSunViewVec);
	vec3 V = normalize(inViewVec);
	vec3 R = normalize(-reflect(L, N));

	outColour = baseColour;
}