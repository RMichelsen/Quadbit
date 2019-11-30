#pragma once

#include <EASTL/chrono.h>

namespace Quadbit::Time {
	using clock = eastl::chrono::high_resolution_clock;

	inline auto tStart = eastl::chrono::time_point_cast<eastl::chrono::nanoseconds>(clock::now());
	inline float deltaTime;

	inline void UpdateTimer() {
		auto tEnd = clock::now();

		// Update deltatime
		deltaTime = static_cast<eastl::chrono::duration<float, eastl::ratio<1>>>(tEnd - tStart).count();
		tStart = clock::now();
	}
}
