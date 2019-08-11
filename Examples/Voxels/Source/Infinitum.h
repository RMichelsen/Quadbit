#pragma once

#include <Windows.h>

#include "Extern/FastNoiseSIMD/FastNoiseSIMD.h"

#include "Engine/Rendering/QbVkRenderer.h"
#include "Engine/Entities/EntityManager.h"

class Infinitum {
public:
	Infinitum(HINSTANCE hInstance, HWND hwnd) {
		renderer_ = std::make_unique<Quadbit::QbVkRenderer>(hInstance, hwnd);
	}

	void Init();
	void Simulate(float deltaTime);
	void DrawFrame();

private:
	std::unique_ptr<Quadbit::QbVkRenderer> renderer_;
	std::shared_ptr<Quadbit::QbVkRenderMeshInstance> renderMeshInstance_;
	FastNoiseSIMD* fastnoise_;

	Quadbit::Entity camera_;
	Quadbit::Entity player_;
};