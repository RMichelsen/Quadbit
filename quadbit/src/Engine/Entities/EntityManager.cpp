#include "Engine/Entities/EntityManager.h"
#include "Engine/Entities/SystemDispatch.h"

namespace Quadbit {
	Entity::Entity() : id_(0, 1) {}

	void Entity::Destroy() {
		EntityManager::Instance().Destroy(*this);
	}

	bool Entity::IsValid() {
		return id_.version == EntityManager::Instance().GetEntityVersion(id_);
	}

	EntityManager::EntityManager() {
		systemDispatch_ = std::make_unique<SystemDispatch>();
	}

	EntityManager& EntityManager::Instance() {
		static EntityManager instance;
		return instance;
	}

	Entity EntityManager::Create() {
		// If the freelist is empty, just add a new entity with version 1
		if(entityFreeList_.empty()) {
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
		for(auto&& pool : componentPools_) {
			if(pool == nullptr) break;
			pool->RemoveIfExists(entity.id_);
		}

		// Remove by swap and pop
		auto lastEntity = entities_.back();
		sparse_[entities_.back().id_.index] = sparse_[entity.id_.index];
		std::swap(entities_[sparse_[entity.id_.index]], entities_.back());
		entities_.pop_back();
		sparse_[entity.id_.index] = 0xFFFFFFFF;

		entityVersions_[entity.id_.index]++;
		entityFreeList_.push_front(entity.id_.index);
	}

	bool EntityManager::IsValid(const Entity& entity) {
		return entity.id_.version == entityVersions_[entity.id_.index];
	}
}