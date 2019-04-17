#pragma once
#include "Entity.h"
#include "Component.h"
#include "../Engine/Application/Logging.h"

namespace Quadbit {
	class EntityManager {
	public:
		Entity Create();

		std::array<std::shared_ptr<void>, MAX_COMPONENTS> componentPools_;

		template<class T>
		void RegisterComponent() {
			//static_assert(std::is_base_of_v<Component, T>, "Components must inherit from Component");
			if(componentPools_[ComponentID::GetUnique<T>()] != nullptr) {
				QB_LOG_INFO("Fail doge");
				return;
			}
			componentPools_[ComponentID::GetUnique<T>()] = std::make_shared<SparseSet<T>>();
		}

	private:
		uint32_t nextEntityId_ = 0;

	};
}