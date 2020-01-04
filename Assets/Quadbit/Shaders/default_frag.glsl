#version 460

layout(binding = 1) uniform sampler2DShadow shadowMap;
layout(binding = 2) uniform sampler2D baseColourMap;
layout(binding = 3) uniform sampler2D metallicRoughnessMap;
layout(binding = 4) uniform sampler2D normalMap;
layout(binding = 5) uniform sampler2D occlusionMap;
layout(binding = 6) uniform sampler2D emissiveMap;

layout(binding = 7) uniform MaterialUBO {
	vec4 baseColourFactor;
	vec4 emissiveFactor;
	float metallicFactor;
	float roughnessFactor;
	float alphaMask;
	float alphaMaskCutoff;
	int baseColourTextureIndex;
	int metallicRoughnessTextureIndex;
	int normalTextureIndex;
	int occlusionTextureIndex;
	int emissiveTextureIndex;
} ubo;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV0;
layout(location = 3) in vec2 inUV1;
layout(location = 4) in vec3 inViewVec;
layout(location = 5) in vec3 inSunViewVec;
layout(location = 6) in vec4 inLightSpaceCoords;

layout(location = 0) out vec4 outColour;

vec4 LinearFromSRGB(vec4 srgb) {
	return vec4(pow(srgb.xyz, vec3(2.2)), srgb.w);
}

// http://www.thetenthplanet.de/archives/1180
vec3 GetNormal() {
	vec3 tangentNormal = texture(normalMap, ubo.normalTextureIndex == 0 ? inUV0 : inUV1).xyz * 2.0 - 1.0;

	vec3 q1 = dFdx(inPos);
	vec3 q2 = dFdy(inPos);
	vec2 st1 = dFdx(inUV0);
	vec2 st2 = dFdy(inUV0);

	vec3 T = normalize(q1 * st2.t - q2 * st1.t);
	vec3 N = normalize(inNormal);
	vec3 B = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return normalize(TBN * tangentNormal);
}

#define ambient 0.2

// float CalcShadow(vec4 shadowCoord) {
// 	float shadow = 1.0;
// 	if(shadowCoord.z > -1.0 && shadowCoord.z < 1.0) {
// 		float dist = texture(shadowMap, shadowCoord.xy).r;
// 		if(shadowCoord.w > 0.0 && dist < shadowCoord.z) {
// 			shadow = ambient;
// 		}
// 	}
// 	return shadow;
// }

float CalcShadow(vec4 shadowCoord) {
	vec2 textureSize = textureSize(shadowMap, 0);
	vec2 deltas = vec2(1.0 / textureSize.x, 1.0 / textureSize.y);

	vec3 p = shadowCoord.xyz / shadowCoord.w;

	float light = 0.0;
	for(int x = -1; x <= 1; x++) {
		for(int y = -1; y <= 1; y++) {
			vec2 offsets = vec2(x * deltas.x, y * deltas.y);
			vec3 sampleCoord = vec3(p.xy + offsets, p.z + 0.00001);
			light += texture(shadowMap, sampleCoord);
		}
	}
	return ambient + (light / 18.0);
}



float CalculateSmoothAttenuation(vec3 p, vec3 l) {
	vec3 dir = l - p;
	float r2 = length(dir);
	// vec2 attenConst = vec2(1 / 10^2, 2 / 10);
	float attenuation = clamp(2.0 / r2, 0.0, 1.0);
	return attenuation;
}

void main() {
	vec4 baseColour = (ubo.baseColourTextureIndex > -1) ?
		texture(baseColourMap, ubo.baseColourTextureIndex == 0 ? inUV0 : inUV1) : 
		ubo.baseColourFactor;

	vec3 normal = (ubo.normalTextureIndex > -1) ?
		GetNormal() : inNormal;

	if(ubo.alphaMask == 1.0f) {
		if(baseColour.a < ubo.alphaMaskCutoff) {
			discard;
		}
		else {
			baseColour *= ubo.baseColourFactor;
		}
	}

	if(ubo.emissiveTextureIndex > -1) {
		vec4 emissive = LinearFromSRGB(texture(emissiveMap, ubo.emissiveTextureIndex == 0 ? inUV0 : inUV1)) * ubo.emissiveFactor;
		baseColour += emissive;
	}

	vec3 N = normalize(normal);
	vec3 L = normalize(vec3(200.0f, 300.0f, 200.0f));
	vec3 V = normalize(inViewVec);
	vec3 R = normalize(-reflect(L, N));

	vec3 diffuse = max(dot(normal, L), 0.0) * baseColour.xyz;
	float shadow = CalcShadow(inLightSpaceCoords);

	vec3 plight = vec3(0.9, 0.85, 0.7) * 1.0 * CalculateSmoothAttenuation(inPos, vec3(5.0, 5.0, 5.0));


	outColour = vec4(plight + (diffuse * shadow), 1.0);
	//vec2 screenRelativeCoord = vec2(gl_FragCoord.x / 2538.0, 1 - gl_FragCoord.y / 1384.0);
	//outColour = vec4(shadow * 0.6, 0, 0, 1);
}