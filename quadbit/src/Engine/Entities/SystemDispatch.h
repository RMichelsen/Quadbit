#pragma once
#include <vector>
#include <imgui/imgui.h>

#include "Engine/Core/QbEntityDefs.h"
#include "Engine/Core/Sfinae.h"
#include "Engine/Entities/ComponentSystem.h"
#include "Engine/Entities/EntityManager.h"

namespace Quadbit {
	template <class S>
	using has_init = decltype(std::declval<S>().Init());

	class SystemDispatch {
	public:
		std::size_t systemCount_ = 0;
		std::array<std::shared_ptr<ComponentSystem>, MAX_SYSTEMS> systems_;

		void Shutdown() {
			for(auto&& system : systems_) {
				system.reset();
			}
		}

		template<typename S>
		void RegisterSystem() {
			static_assert(std::is_base_of_v<ComponentSystem, S>, "All systems must inherit from ComponentSystem");
			size_t systemID = SystemID::GetUnique<S>();
			systems_[systemID] = std::make_shared<S>();
			systems_[systemID]->name = typeid(S).name();
			if constexpr(SFINAE::is_detected_v<has_init, S>) {
				std::static_pointer_cast<S>(systems_[systemID])->Init();
			}
			systemCount_++;
		}

		template<typename S, typename... Args>
		void RunSystem(float deltaTime = 0.0f, Args... args) {
			size_t systemID = SystemID::GetUnique<S>();
			if(systems_[systemID] == nullptr) {
				RegisterSystem<S>();
			}
			auto ptr = std::static_pointer_cast<S>(systems_[systemID]);
			ptr->UpdateStart();
			ptr->Update(deltaTime, args...);
			ptr->UpdateEnd();
		}

		void ImGuiDrawState() {
			ImGui::SetNextWindowSize(ImVec2(500, 200), ImGuiCond_FirstUseEver);
			ImGui::Begin("ECS System Status", nullptr);

			for(auto i = 0; i < systemCount_; i++) {
				ImGui::Text("%s %.5fms", systems_[i]->name, systems_[i]->deltaTime);
			}

			ImGui::End();
		}
	};
}
