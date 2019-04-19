#pragma once

#include "../Engine/Rendering/QbVkRenderer.h"

class Infinitum {
public:
	Infinitum(HINSTANCE hInstance, HWND hwnd) {
		entityManager_ = std::make_shared<Quadbit::EntityManager>();
		renderer_ = std::make_unique<Quadbit::QbVkRenderer>(hInstance, hwnd, entityManager_);
	}

	void Init();
	void Simulate(float deltaTime);
	void DrawFrame();

private:
	std::shared_ptr<Quadbit::EntityManager> entityManager_;
	std::unique_ptr<Quadbit::QbVkRenderer> renderer_;

};