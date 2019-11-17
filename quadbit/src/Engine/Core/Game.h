#pragma once

#include "Engine/Application/InputHandler.h"
#include "Engine/Entities/EntityManager.h"
#include "Engine/Rendering/Renderer.h"
#include "Engine/API/Compute.h"
#include "Engine/API/Graphics.h"

namespace Quadbit {
	class Game {
	public:
		InputHandler* input_;
		EntityManager* entityManager_;
		Graphics* graphics_;
		Compute* compute_;

		virtual ~Game() {
			delete graphics_;
			delete compute_;
		};

		virtual void Init() = 0;
		virtual void Simulate(float deltaTime) = 0;
	};
}