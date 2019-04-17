#pragma once

#include <vector>
#include <functional>

#define IMGUI_VAR(x) ImGuiState::pointers[ImGuiState::varCount - x]

namespace Quadbit::ImGuiState {
	inline uint32_t varCount = 0;
	inline std::vector<std::function<void()>> injectors;
	inline std::vector<void*> pointers;

	// Inject takes a functor with ImGui commands and an optional
	// list of local variable injections that are in turn stored in
	// the global state as pointers. To address them in the functor use
	// the macro IMGUI_VAR and pass the index of the variable in the 
	// argument list in REVERSE order!
	template<typename... T>
	inline void Inject(std::function<void()> functor, T... injections) {
		for(auto&& x : { injections... }) {
			pointers.push_back(x);
			varCount++;
		}
		injectors.push_back(functor);
	}
}
