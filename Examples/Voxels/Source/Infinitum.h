#pragma once
#include <Windows.h>

#include "Extern/FastNoiseSIMD/FastNoiseSIMD.h"

#include "Engine/Rendering/QbVkRenderer.h"
#include "Engine/Entities/EntityManager.h"

struct FastNoiseSettings {
	int seed = 4646;
	int noiseType = static_cast<int>(FastNoiseSIMD::NoiseType::Simplex);
	float noiseFrequency = 0.032f;
	int fractalType = static_cast<int>(FastNoiseSIMD::FractalType::FBM);
	int fractalOctaves = 5;
	float fractalLacunarity = 2.0f;
	float fractalGain = 0.5f;
	int perturbType = static_cast<int>(FastNoiseSIMD::PerturbType::None);
	int timer = 0;
};

class Infinitum {
public:
	Infinitum(HINSTANCE hInstance, HWND hwnd) {
		renderer_ = std::make_unique<Quadbit::QbVkRenderer>(hInstance, hwnd);
	}
	~Infinitum() {
		delete fastnoiseTerrain_;
		delete fastnoiseRegions_;
		delete fastnoiseColours_;
	}

	inline static FastNoiseSettings terrainSettings_;
	inline static FastNoiseSettings colourSettings_;

	void Test();
	void Init();
	void Simulate(float deltaTime);
	void DrawFrame();

private:
	std::unique_ptr<Quadbit::QbVkRenderer> renderer_;
	std::shared_ptr<Quadbit::QbVkRenderMeshInstance> renderMeshInstance_;
	FastNoiseSIMD* fastnoiseTerrain_;
	FastNoiseSIMD* fastnoiseRegions_;
	FastNoiseSIMD* fastnoiseColours_;

	std::vector<Quadbit::Entity> chunks_;

	Quadbit::Entity camera_;
	Quadbit::Entity player_;
};