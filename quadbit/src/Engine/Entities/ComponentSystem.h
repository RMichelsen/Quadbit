#pragma once
#include <chrono>

namespace Quadbit {
	struct ComponentSystem {
		using clock = std::chrono::high_resolution_clock;
		float deltaTime = 0.0f;
		std::chrono::time_point<clock> tStart;
		const char* name = nullptr;

		ComponentSystem() {
			tStart = std::chrono::time_point_cast<std::chrono::nanoseconds>(clock::now());
		}

		virtual ~ComponentSystem() {}

		void UpdateStart() {
			tStart = clock::now();
		}

		void UpdateEnd() {
			auto tEnd = clock::now();

			// Update deltatime
			deltaTime = static_cast<std::chrono::duration<float, std::ratio<1>>>(tEnd - tStart).count();
		}
	};
}