#pragma once

namespace Time {
	using clock = std::chrono::high_resolution_clock;

	inline auto tStart = std::chrono::time_point_cast<std::chrono::nanoseconds>(clock::now());
	inline float deltaTime;

	inline void UpdateTimer() {
		auto tEnd = clock::now();

		// Update deltatime
		deltaTime = static_cast<std::chrono::duration<float, std::ratio<1>>>(tEnd - tStart).count();
		tStart = clock::now();
	}
}