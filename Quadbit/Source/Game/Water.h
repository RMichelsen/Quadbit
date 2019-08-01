#pragma once

#include "../Engine/Rendering/QbVkRenderer.h"
#include "../Engine/Rendering/Common/QbVkDefines.h"
#include "../Engine/Entities/EntityManager.h"
#include "../Engine/Rendering/Pipelines/ComputePipeline.h"


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
	int useNormalMap;
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
	inline static bool useNormalMap_ = false;

	Water(HINSTANCE hInstance, HWND hwnd) {
		renderer_ = std::make_unique<Quadbit::QbVkRenderer>(hInstance, hwnd);
	}

	void Init();
	void InitializeCompute();
	void RecordComputeCommands();
	void Simulate(float deltaTime);
	void DrawFrame();

private:
	std::unique_ptr<Quadbit::QbVkRenderer> renderer_;

	Quadbit::Entity camera_;

	std::vector<glm::float3> waterVertices_;
	std::vector<uint32_t> waterIndices_;

	PreCalculatedResources precalcResources_{};
	WaveheightResources waveheightResources_{};
	InverseFFTResources horizontalIFFTResources_{};
	InverseFFTResources verticalIFFTResources_{};
	DisplacementResources displacementResources_{};
	Quadbit::QbVkComputeInstance precalcInstance_;
	Quadbit::QbVkComputeInstance waveheightInstance_;
	Quadbit::QbVkComputeInstance horizontalIFFTInstance_;
	Quadbit::QbVkComputeInstance verticalIFFTInstance_;
	Quadbit::QbVkComputeInstance displacementInstance_;

	Quadbit::QbVkBuffer togglesUBO_;

	void InitPrecalcComputeInstance();
	void InitWaveheightComputeInstance();
	void InitInverseFFTComputeInstances();
	void InitDisplacementInstance();
	void UpdateWaveheightUBO(float deltaTime);
};