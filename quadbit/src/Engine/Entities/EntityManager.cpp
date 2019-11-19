#include "Engine/Entities/EntityManager.h"
#include "Engine/Entities/SystemDispatch.h"

namespace Quadbit {
	Entity::Entity() : id_(0, 1) {}

	EntityManager::EntityManager() : systemDispatch_(eastl::make_unique<SystemDispatch>(this)) {}

	EntityManager::~EntityManager() {
		systemDispatch_->Shutdown();
		systemDispatch_.reset();
		for (auto&& pool : componentPools_) {
			// We can break at the first null-pointer since component pools
			// cannot be unregistered (destroyed) at runtime and thus when we
			// encounter a nullptr, no pools are left in the array.
			if (pool.get() == nullptr) break;
			pool.reset();
		}
	}

	Entity EntityManager::Create() {
		// If the freelist is empty, just add a new entity with version 1
		if (entityFreeList_.empty()) {
			auto entity = Entity(EntityID(nextEntityId_, entityVersions_[nextEntityId_]));
			sparse_[entity.id_.index] = static_cast<uint32_t>(entities_.size());
			entities_.push_back(entity);

			nextEntityId_++;
			return entity;
		}
		// Otherwise we use a free index from the freelist
		else {
			auto index = entityFreeList_.front();
			entityFreeList_.pop_front();

			auto entity = Entity(EntityID(index, entityVersions_[index]));
			sparse_[index] = static_cast<uint32_t>(entities_.size());
			entities_.push_back(entity);
			return entity;
		}
	}

	void EntityManager::Destroy(const Entity& entity) {
		// Destroy component pools one by one
		for (auto&& pool : componentPools_) {
			// We can break at the first null-pointer since component pools
			// cannot be unregistered (destroyed) at runtime and thus when we
			// encounter a nullptr, no pools are left in the array.
			if (pool.get() == nullptr) break;
			pool->RemoveIfExists(entity.id_);
		}

		// Remove by swap and pop
		auto lastEntity = entities_.back();
		sparse_[lastEntity.id_.index] = sparse_[entity.id_.index];
		eastl::swap(entities_[sparse_[entity.id_.index]], lastEntity);
		entities_.pop_back();
		sparse_[entity.id_.index] = 0xFFFFFFFF;

		entityVersions_[entity.id_.index]++;
		entityFreeList_.push_front(entity.id_.index);
	}

	bool EntityManager::IsValid(const Entity& entity) {
		return entity.id_.version == entityVersions_[entity.id_.index];
	}
}