#pragma once
#include <vector>
#include <imgui/imgui.h>

#include "Engine/Core/Sfinae.h"
#include "Engine/Entities/QbEntityDefs.h"
#include "Engine/Entities/ComponentSystem.h"
#include "Engine/Entities/EntityManager.h"

namespace Quadbit {
	template <class S>
	using has_init = decltype(std::declval<S>().Init());

	class SystemDispatch {
	public:
		std::size_t systemCount_ = 0;
		std::array<std::unique_ptr<ComponentSystem>, MAX_SYSTEMS> systems_;

		void Shutdown() {
			for (auto&& system : systems_) {
				system.reset();
			}
		}

		template<typename S>
		void RegisterSystem() {
			static_assert(std::is_base_of_v<ComponentSystem, S>, "All systems must inherit from ComponentSystem");
			size_t systemID = SystemID::GetUnique<S>();
			systems_[systemID] = std::make_unique<S>();
			systems_[systemID]->name = typeid(S).name();
			if constexpr (SFINAE::is_detected_v<has_init, S>) {
				reinterpret_cast<S*>(systems_[systemID].get())->Init();
			}
			systemCount_++;
		}

		template<typename S, typename... Args>
		void RunSystem(float deltaTime, Args&&... args) {
			size_t systemID = SystemID::GetUnique<S>();
			if (systems_[systemID] == nullptr) {
				RegisterSystem<S>();
			}
			auto* ptr = reinterpret_cast<S*>(systems_[systemID].get());
			ptr->UpdateStart();
			ptr->Update(deltaTime, args...);
			ptr->UpdateEnd();
		}

		void ImGuiDrawState() {
			ImGui::SetNextWindowSize(ImVec2(500, 200), ImGuiCond_FirstUseEver);
			ImGui::Begin("ECS System Status", nullptr);

			for (auto i = 0; i < systemCount_; i++) {
				ImGui::Text("%s %.5fms", systems_[i]->name, systems_[i]->deltaTime);
			}

			ImGui::End();
		}
	};
}
