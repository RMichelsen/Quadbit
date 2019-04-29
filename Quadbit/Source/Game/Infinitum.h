#pragma once

#include "../Engine/Rendering/QbVkRenderer.h"
#include "../Engine/Entities/EntityManager.h"
#include "../Engine/Physics/PhysicsWorld.h"


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
	FastNoiseSIMD* fastnoise_;
	Quadbit::PhysicsWorld* physicsWorld_;

	Quadbit::Entity camera_;
	Quadbit::Entity player_;
};