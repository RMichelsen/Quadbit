#pragma once

namespace Quadbit {
	class Game {
	public:
		virtual ~Game() {};

		virtual void Init() = 0;
		virtual void Simulate(float deltaTime) = 0;
	};
}