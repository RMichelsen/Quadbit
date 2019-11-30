#pragma once

#include <cstdint>

#include <EASTL/array.h>
#include <EASTL/type_traits.h>
#include <EASTL/unique_ptr.h>
#include <imgui/imgui.h>

#include "Engine/Core/Sfinae.h"
#include "Engine/Entities/EntityManager.h"

namespace Quadbit {
	template <class S>
	using has_init = decltype(eastl::declval<S>().Init());

	class SystemDispatch {
	public:
		EntityManager* const entityManager_;
		size_t systemCount_ = 0;
		eastl::array<eastl::unique_ptr<ComponentSystem>, MAX_SYSTEMS> systems_;

		SystemDispatch(EntityManager* const entityManager) : entityManager_(entityManager) {}

		void Shutdown() {
			for (int i = 0; i < systemCount_; i++) {
				systems_[i].reset();
			}
		}

		template<typename S>
		void RegisterSystem() {
			static_assert(eastl::is_base_of_v<ComponentSystem, S>, "All systems must inherit from ComponentSystem");
			size_t systemID = SystemID::GetUnique<S>();
			systems_[systemID] = eastl::make_unique<S>();
			systems_[systemID]->entityManager_ = entityManager_;
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
