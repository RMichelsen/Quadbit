#version 460

layout(binding = 1) uniform sampler2D shadowMap;
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

layout(location = 0) out vec4 outColour;

vec4 LinearFromSRGB(vec4 srgb) {
	return vec4(pow(srgb.xyz, vec3(2.2)), srgb.w);
}

vec3 getNormal()
{
	// Perturb normal, see http://www.thetenthplanet.de/archives/1180
	vec3 tangentNormal = texture(normalMap, ubo.normalTextureIndex == 0 ? inUV0 : inUV1).xyz * 2.0 - 1.0;

	vec3 q1 = dFdx(inPos);
	vec3 q2 = dFdy(inPos);
	vec2 st1 = dFdx(inUV0);
	vec2 st2 = dFdy(inUV0);

	vec3 N = normalize(inNormal);
	vec3 T = normalize(q1 * st2.t - q2 * st1.t);
	vec3 B = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return normalize(TBN * tangentNormal);
}

void main() {
	vec4 baseColour = (ubo.baseColourTextureIndex > -1) ?
		texture(baseColourMap, ubo.baseColourTextureIndex == 0 ? inUV0 : inUV1) : 
		ubo.baseColourFactor;

	vec3 normal = (ubo.normalTextureIndex > -1) ?
		getNormal() : inNormal;

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
	vec3 L = normalize(vec3(0.5f, 1.0f, 0.2f));
	vec3 V = normalize(inViewVec);
	vec3 R = normalize(-reflect(L, N));

	float ambient = 0.1f;
	float diffuse = max(dot(N, L), 0.0);

	outColour = vec4((ambient + diffuse) * baseColour.xyz, 1.0);
	// outColour = vec4(normal, 1.0);
}