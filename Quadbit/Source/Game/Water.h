#pragma once

#include "../Engine/Rendering/QbVkRenderer.h"
#include "../Engine/Entities/EntityManager.h"


class Water {
public:
	Water(HINSTANCE hInstance, HWND hwnd) {
		renderer_ = std::make_unique<Quadbit::QbVkRenderer>(hInstance, hwnd);
	}

	void Init();
	void Simulate(float deltaTime);
	void DrawFrame();

private:
	std::unique_ptr<Quadbit::QbVkRenderer> renderer_;

	Quadbit::Entity camera_;
};