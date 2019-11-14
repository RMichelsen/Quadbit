#pragma once
#include <Windows.h>

#include <FastNoiseSIMD/FastNoiseSIMD.h>

#include "Engine/Core/Entry.h"
#include "Engine/Core/Game.h"
#include "Engine/Entities/EntityManager.h"
#include "Engine/Rendering/Renderer.h"

constexpr char* NOISE_TYPES[] = { "Value", "ValueFractal", "Perlin", "PerlinFractal", "Simplex", "SimplexFractal", "WhiteNoise", "Cellular", "Cubic", "CubicFractal" };
constexpr char* FRACTAL_TYPES[] = { "FBM", "Billow", "RigidMulti" };
constexpr char* PERTURB_TYPES[] = { "None", "Gradient", "Gradient Fractal", "Normalize", "Gradient + Normalize", "Gradient Fractal + Normalize" };

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

class Infinitum : public Quadbit::Game {
public:
	virtual ~Infinitum() override {
		delete fastnoiseTerrain_;
		delete fastnoiseRegions_;
		delete fastnoiseColours_;
	}

	FastNoiseSettings terrainSettings_;
	FastNoiseSettings colourSettings_;

	void Init() override;
	void Simulate(float deltaTime) override;
	void DrawFrame();

private:
	const Quadbit::QbVkRenderMeshInstance* renderMeshInstance_;
	FastNoiseSIMD* fastnoiseTerrain_;
	FastNoiseSIMD* fastnoiseRegions_;
	FastNoiseSIMD* fastnoiseColours_;

	std::vector<Quadbit::Entity> chunks_;

	Quadbit::Entity camera_;
	Quadbit::Entity player_;

	void DrawImGui();
};

Quadbit::Game* Quadbit::CreateGame() {
	return new Infinitum();
}