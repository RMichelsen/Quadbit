#pragma once

#include <Windows.h>
#include <glm/gtx/compatibility.hpp>

#include "Engine/Rendering/QbVkRenderer.h"
#include "Engine/Entities/EntityManager.h"

constexpr int WATER_RESOLUTION = 512;
constexpr VkFormat IMAGE_FORMAT = VK_FORMAT_R32G32B32A32_SFLOAT;

struct alignas(16) PrecalcUBO {
	glm::float2 W;
	int N;
	float A;
	int L;
};

struct alignas(16) WaveheightUBO {
	int N;
	int L;
	float RT;
	float T;
};

struct alignas(16) TogglesUBO {
	glm::float4 topColour;
	glm::float4 botColour;
	glm::float3 cameraPos;
	int useNormalMap;
	float colourIntensity;
};

struct PreCalculatedResources {
	std::vector<glm::float4> precalcUniformRandoms;

	Quadbit::QbVkBuffer ubo;
	Quadbit::QbVkBuffer uniformRandomsStorageBuffer;

	Quadbit::QbVkTexture h0Tilde;
	Quadbit::QbVkTexture h0TildeConj;
};

struct WaveheightResources {
	Quadbit::QbVkBuffer ubo;

	Quadbit::QbVkTexture h0TildeTx;
	Quadbit::QbVkTexture h0TildeTy;
	Quadbit::QbVkTexture h0TildeTz;
	Quadbit::QbVkTexture h0TildeSlopeX;
	Quadbit::QbVkTexture h0TildeSlopeZ;
};

struct IFFTPushConstants {
	int iteration;
};

struct InverseFFTResources {
	Quadbit::QbVkTexture dX{};
	Quadbit::QbVkTexture dY{};
	Quadbit::QbVkTexture dZ{};
	Quadbit::QbVkTexture dSlopeX{};
	Quadbit::QbVkTexture dSlopeZ{};

	std::array<int, 6> specData;

	IFFTPushConstants pushConstants;
};

struct DisplacementResources {
	Quadbit::QbVkTexture displacementMap;
	Quadbit::QbVkTexture normalMap;
};

class Water {
public:
	inline static float step_;
	inline static float repeat_;

	inline static bool useNormalMap_ = true;
	inline static float colourIntensity_ = 0.0f;
	inline static glm::float4 topColour_ = glm::float4(0.15f, 0.7f, 0.8f, 0.0f);
	inline static glm::float4 botColour_ = glm::float4(0.10f, 0.45f, 0.7f, 0.0f);

	Water(HINSTANCE hInstance, HWND hwnd) {
		renderer_ = std::make_unique<Quadbit::QbVkRenderer>(hInstance, hwnd);
	}

	void Init();
	void InitializeCompute();
	void RecordComputeCommands();
	void Simulate(float deltaTime);
	void DrawFrame();

private:

	Quadbit::Entity skybox;

	std::unique_ptr<Quadbit::QbVkRenderer> renderer_;

	std::vector<glm::float3> waterVertices_;
	std::vector<uint32_t> waterIndices_;

	PreCalculatedResources precalcResources_{};
	WaveheightResources waveheightResources_{};
	InverseFFTResources horizontalIFFTResources_{};
	InverseFFTResources verticalIFFTResources_{};
	DisplacementResources displacementResources_{};
	Quadbit::QbVkComputeInstance* precalcInstance_ = nullptr;
	Quadbit::QbVkComputeInstance* waveheightInstance_ = nullptr;
	Quadbit::QbVkComputeInstance* horizontalIFFTInstance_ = nullptr;
	Quadbit::QbVkComputeInstance* verticalIFFTInstance_ = nullptr;
	Quadbit::QbVkComputeInstance* displacementInstance_ = nullptr;

	Quadbit::QbVkBuffer togglesUBO_;

	void InitPrecalcComputeInstance();
	void InitWaveheightComputeInstance();
	void InitInverseFFTComputeInstances();
	void InitDisplacementInstance();
	void UpdateWaveheightUBO(float deltaTime);
	void UpdateTogglesUBO(float deltaTime);
};