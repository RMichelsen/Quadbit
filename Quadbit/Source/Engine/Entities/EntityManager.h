#pragma once
#include "EntitiesCommon.h"
#include "ComponentPool.h"
#include "Entity.h"

namespace Quadbit {
	class EntityManager {
	public:
		std::array<std::shared_ptr<ComponentPool>, MAX_COMPONENTS> componentPools_;

		Entity Create();
		void Destroy(const Entity& entity);
		bool IsValid(const Entity& entity);

		template<typename C>
		void RegisterComponent() {
			size_t componentID = ComponentID::GetUnique<C>();
			assert(componentPools_[componentID] == nullptr && "Failed to add component: Component isn't registered with the entity manager\n");
			componentPools_[componentID] = std::make_shared<SparseSet<C>>();
		}

		template<typename C>
		void AddComponent(const Entity& entity) const {
			size_t componentID = ComponentID::GetUnique<C>();
			assert(componentPools_[componentID] != nullptr && "Failed to add component: Component isn't registered with the entity manager\n");
			std::static_pointer_cast<SparseSet<C>>(componentPools_[componentID])->Insert(entity.id_);
		}
		// Aggregate initialization
		template<typename C>
		void AddComponent(const Entity& entity, C&& t) const {
			size_t componentID = ComponentID::GetUnique<C>();
			assert(componentPools_[componentID] != nullptr && "Failed to add component: Component isn't registered with the entity manager\n");
			std::static_pointer_cast<SparseSet<C>>(componentPools_[componentID])->Insert(entity.id_, std::move(t));
		}

		template<typename C>
		C* const GetComponent(const Entity& entity) const {
			return std::static_pointer_cast<SparseSet<C>>(componentPools_[ComponentID::GetUnique<C>()])->GetComponent(entity.id_);
		}

		template<typename C>
		void RemoveComponent(const Entity& entity) const {
			size_t componentID = ComponentID::GetUnique<C>();
			assert(componentPools_[componentID] != nullptr && "Failed to add component: Component isn't registered with the entity manager\n");
			std::static_pointer_cast<SparseSet<C>>(componentPools_[componentID])->Remove(entity.id_);
		}

		template<typename C>
		std::shared_ptr<SparseSet<C>> GetPool() {
			size_t componentID = ComponentID::GetUnique<C>();
			assert(componentPools_[componentID] != nullptr && "Failed to add component: Component isn't registered with the entity manager\n");
			return std::static_pointer_cast<SparseSet<C>>(componentPools_[componentID]);
		}

		template<typename... Components>
		std::vector<uint32_t>& SmallestPoolEntityFromComponentIndices() {
			std::size_t sizes[]{ GetPool<Components>()->dense_.size()... };
			std::vector<uint32_t>* vectors[]{ &GetPool<Components>()->entityFromComponentIndices_... };

			auto smallest_size_ptr = std::min_element(std::begin(sizes), std::end(sizes));
			return *vectors[smallest_size_ptr - sizes];
		}

		template<typename C>
		using sparseSetPtr = std::shared_ptr<SparseSet<C>>;

		template<typename... Components, typename F>
		void ForEach(F fun) {
			std::tuple pools{ GetPool<Components>()... };

			// Get the smallest pool to iterate from as a base
			auto entityFromComponentIndices = SmallestPoolEntityFromComponentIndices<Components...>();

			for(auto entityIndex : entityFromComponentIndices) {
				const bool allValid = ((std::get<sparseSetPtr<Components>>(pools)->sparse_[entityIndex] != 0xFFFF'FFFF) && ...);
				if(!allValid) continue;

				fun(std::get<sparseSetPtr<Components>>(pools)->dense_[std::get<sparseSetPtr<Components>>(pools)->sparse_[entityIndex]]...);
			}
		}

		template<typename... Components, typename F>
		void ParForEach(F fun) {
			std::tuple pools{ GetPool<Components>()... };

			// Get the smallest pool to iterate from as a base
			auto entityFromComponentIndices = SmallestPoolEntityFromComponentIndices<Components...>();

			std::for_each(std::execution::par, entityFromComponentIndices.begin(), entityFromComponentIndices.end(), [&fun, &pools](auto entityIndex) {
				const bool allValid = ((std::get<sparseSetPtr<Components>>(pools)->sparse_[entityIndex] != 0xFFFF'FFFF) && ...);
				if(!allValid) return;

				fun(std::get<sparseSetPtr<Components>>(pools)->dense_[std::get<sparseSetPtr<Components>>(pools)->sparse_[entityIndex]]...);
			});
		}

	private:
		// Not sure if public makes more sense regardless, friend class for now
		friend class Entity;

		uint32_t nextEntityId_ = 0;

		std::vector<uint32_t> sparse_ = std::vector<uint32_t>(INIT_MAX_ENTITIES, 0xFFFFFFFF);
		std::vector<uint32_t> entityVersions_ = std::vector<uint32_t>(INIT_MAX_ENTITIES, 1);
		std::vector<Entity> entities_;

		// Holds free entity indices
		std::deque<uint32_t> entityFreeList_;
	};
}