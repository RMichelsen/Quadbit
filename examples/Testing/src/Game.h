#pragma once

#include <Windows.h>
#include <glm/gtx/compatibility.hpp>

#include "Engine/Rendering/QbVkRenderer.h"
#include "Engine/Entities/EntityManager.h"

class Game {
public:
	Game(HINSTANCE hInstance, HWND hwnd) {
		renderer_ = std::make_unique<Quadbit::QbVkRenderer>(hInstance, hwnd);
	}

	void Init();
	void Simulate(float deltaTime);
	void DrawFrame();

private:
	std::unique_ptr<Quadbit::QbVkRenderer> renderer_;
};