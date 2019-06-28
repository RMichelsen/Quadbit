#pragma once

#include "../Engine/Rendering/QbVkRenderer.h"
#include "../Engine/Rendering/Common/QbVkDefines.h"
#include "../Engine/Entities/EntityManager.h"
#include "../Engine/Rendering/Pipelines/ComputePipeline.h"


constexpr int WATER_RESOLUTION = 512;

struct WaveheightUBO {
	float time;
	int N;
	glm::float2 W;
	glm::float2 WN;
	float A;
	int L;
};

struct WaveheightResources {
	Quadbit::QbVkBuffer ubo;

	Quadbit::QbVkImage h0s;
	Quadbit::QbVkImage h0sConj;
};

class Water {
public:
	Water(HINSTANCE hInstance, HWND hwnd) {
		renderer_ = std::make_unique<Quadbit::QbVkRenderer>(hInstance, hwnd);
	}

	void Init();
	void InitializeCompute();
	void RecordComputeCommands(Quadbit::QbVkComputeInstance& computeInstance);
	void Simulate(float deltaTime);
	void DrawFrame();

private:
	std::unique_ptr<Quadbit::QbVkRenderer> renderer_;

	Quadbit::Entity camera_;

	std::vector<Quadbit::MeshVertex> waterVertices_;
	std::vector<uint32_t> waterIndices_;

	WaveheightResources waveheightResources_{};
};