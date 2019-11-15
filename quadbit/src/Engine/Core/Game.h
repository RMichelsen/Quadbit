#pragma once

#include "Engine/Application/InputHandler.h"
#include "Engine/Entities/EntityManager.h"
#include "Engine/Rendering/Renderer.h"

namespace Quadbit {
	class Game {
	public:
		InputHandler* input_;
		EntityManager* entityManager_;
		QbVkRenderer* renderer_;

		virtual ~Game() {};

		virtual void Init() = 0;
		virtual void Simulate(float deltaTime) = 0;
	};
}