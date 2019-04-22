#pragma once

#include "EntitiesCommon.h"

namespace Quadbit {
	class EntityManager;
	class Entity {
	public:
		EntityManager* manager_;
		EntityID id_;

		Entity() : manager_(nullptr), id_(0, 1) {}
		Entity(EntityManager* entityManager, EntityID id) : manager_(entityManager), id_(id) {}

		void Destroy();
		bool IsValid();

		template<typename T>
		void AddComponent() {
			manager_->AddComponent<T>(*this);
		}
		// Aggregate initialization
		template<typename T>
		void AddComponent(T&& t) {
			manager_->AddComponent<T>(*this, std::move(t));
		}

		template<typename T>
		T* const GetComponentPtr() {
			return manager_->GetComponentPtr<T>(*this);
		}

		template<typename T>
		void RemoveComponent() {
			manager_->RemoveComponent<T>(*this);
		}

	private:
	};
}