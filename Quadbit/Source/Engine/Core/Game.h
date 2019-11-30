#pragma once

#include <EASTL/unique_ptr.h>

#include "Engine/Application/InputHandler.h"
#include "Engine/Entities/EntityManager.h"
#include "Engine/API/Compute.h"
#include "Engine/API/Graphics.h"

namespace Quadbit {
	class Game {
	public:
		InputHandler* input_ = nullptr;
		EntityManager* entityManager_ = nullptr;
		Graphics* graphics_ = nullptr;
		Compute* compute_ = nullptr;

		virtual ~Game() = default;
		virtual void Init() = 0;
		virtual void Simulate(float deltaTime) = 0;
	};
}