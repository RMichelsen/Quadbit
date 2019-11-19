#pragma once

#include <Windows.h>
#include <glm/gtx/compatibility.hpp>
#include <EASTL/array.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>

#include "Engine/Core/Entry.h"
#include "Engine/Core/Game.h"
#include "Engine/Rendering/Renderer.h"
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
	eastl::vector<glm::float4> precalcUniformRandoms;

	Quadbit::QbVkMappedBuffer<PrecalcUBO> ubo;
	Quadbit::QbVkBufferHandle uniformRandomsStorageBuffer;

	Quadbit::QbVkTextureHandle h0Tilde;
	Quadbit::QbVkTextureHandle h0TildeConj;
};

struct WaveheightResources {
	Quadbit::QbVkMappedBuffer<WaveheightUBO> ubo;

	Quadbit::QbVkTextureHandle h0TildeTx;
	Quadbit::QbVkTextureHandle h0TildeTy;
	Quadbit::QbVkTextureHandle h0TildeTz;
	Quadbit::QbVkTextureHandle h0TildeSlopeX;
	Quadbit::QbVkTextureHandle h0TildeSlopeZ;
};

struct IFFTPushConstants {
	int iteration;
};

struct InverseFFTResources {
	Quadbit::QbVkTextureHandle dX{};
	Quadbit::QbVkTextureHandle dY{};
	Quadbit::QbVkTextureHandle dZ{};
	Quadbit::QbVkTextureHandle dSlopeX{};
	Quadbit::QbVkTextureHandle dSlopeZ{};

	eastl::array<int, 6> specData;

	IFFTPushConstants pushConstants;
};

struct DisplacementResources {
	Quadbit::QbVkTextureHandle displacementMap;
	Quadbit::QbVkTextureHandle normalMap;
};

class Water : public Quadbit::Game {
public:
	void Init() override;
	void InitializeCompute();
	void RecordComputeCommands();
	void Simulate(float deltaTime) override;

private:
	float step_ = 1.0f;
	float repeat_ = 200.0f;
	bool useNormalMap_ = true;
	float colourIntensity_ = 0.0f;
	glm::float4 topColour_ = glm::float4(0.15f, 0.7f, 0.8f, 0.0f);
	glm::float4 botColour_ = glm::float4(0.10f, 0.45f, 0.7f, 0.0f);

	Quadbit::Entity skybox;

	eastl::vector<glm::float3> waterVertices_;
	eastl::vector<uint32_t> waterIndices_;

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

	Quadbit::QbVkMappedBuffer<TogglesUBO> togglesUBO_;

	void InitPrecalcComputeInstance();
	void InitWaveheightComputeInstance();
	void InitInverseFFTComputeInstances();
	void InitDisplacementInstance();
	void UpdateWaveheightUBO(float deltaTime);
	void UpdateTogglesUBO(float deltaTime);
	void DrawImGui();
};

eastl::unique_ptr<Quadbit::Game> Quadbit::CreateGame() {
	return eastl::make_unique<Water>();
}