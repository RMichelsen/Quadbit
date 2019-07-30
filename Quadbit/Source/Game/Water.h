#pragma once

#include "../Engine/Rendering/QbVkRenderer.h"
#include "../Engine/Rendering/Common/QbVkDefines.h"
#include "../Engine/Entities/EntityManager.h"
#include "../Engine/Rendering/Pipelines/ComputePipeline.h"


constexpr int WATER_RESOLUTION = 1024;
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

struct PreCalculatedResources {
	Quadbit::QbVkBuffer ubo;

	Quadbit::QbVkImage h0Tilde;
	Quadbit::QbVkImage h0TildeConj;

	std::vector<glm::float4> precalcUniformRandoms;
	Quadbit::QbVkBuffer uniformRandomsStorageBuffer;
};

struct WaveheightResources {
	Quadbit::QbVkBuffer ubo;

	Quadbit::QbVkImage h0TildeTx;
	Quadbit::QbVkImage h0TildeTy;
	Quadbit::QbVkImage h0TildeTz;
	Quadbit::QbVkImage h0TildeSlopeX;
	Quadbit::QbVkImage h0TildeSlopeZ;

	VkDescriptorImageInfo h0TildeTxImgInfo{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL };
	VkDescriptorImageInfo h0TildeTyImgInfo{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL };
	VkDescriptorImageInfo h0TildeTzImgInfo{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL };
	VkDescriptorImageInfo h0TildeSlopeXImgInfo{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL };
	VkDescriptorImageInfo h0TildeSlopeZImgInfo{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL };
};

struct IFFTPushConstants {
	int iteration;
};

struct InverseFFTResources {
	Quadbit::QbVkImage Dx;
	Quadbit::QbVkImage Dy;
	Quadbit::QbVkImage Dz;

	Quadbit::QbVkImage DSlopeX;
	Quadbit::QbVkImage DSlopeZ;

	VkDescriptorImageInfo DxImgInfo{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL };
	VkDescriptorImageInfo DyImgInfo{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL };
	VkDescriptorImageInfo DzImgInfo{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL };
	VkDescriptorImageInfo DSlopeXImgInfo{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL };
	VkDescriptorImageInfo DSlopeZImgInfo{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL };

	std::array<int, 6> specData;

	IFFTPushConstants pushConstants;
};

struct DisplacementResources {
	Quadbit::QbVkImage displacementMap;
	Quadbit::QbVkImage normalMap;
	VkSampler normalMapSampler;
};

class Water {
public:
	inline static float step_;
	inline static float repeat_;

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

	std::vector<Quadbit::MeshVertex> waterVertices_;
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

	void InitPrecalcComputeInstance();
	void InitWaveheightComputeInstance();
	void InitInverseFFTComputeInstances();
	void InitDisplacementInstance();
	void UpdateWaveheightUBO(float deltaTime);
};