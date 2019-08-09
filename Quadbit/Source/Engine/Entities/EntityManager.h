#pragma once
#include <deque>


#include "EventTag.h"
#include "ComponentPool.h"
#include "SystemDispatch.h"

namespace Quadbit {
	class EntityManager;
	struct Entity {
		EntityManager* manager_;
		EntityID id_;

		Entity();
		Entity(EntityManager* entityManager, EntityID id) : manager_(entityManager), id_(id) {}

		bool operator == (const Entity& other) const { return (id_ == other.id_) && (manager_ == other.manager_); }
		bool operator != (const Entity& other) const { return (id_ != other.id_) && (manager_ != other.manager_); }

		void Destroy();
		bool IsValid();

		template<typename T>
		void AddComponent();

		// Aggregate initialization
		template<typename T>
		void AddComponent(T&& t);

		template<typename... T>
		void AddComponents();

		template<typename T>
		T* const GetComponentPtr();

		template<typename T>
		bool HasComponent();

		template<typename T>
		void RemoveComponent();
	};

	const Entity NULL_ENTITY{ nullptr, {0xFFFF'FFFF, 0xFFFF'FFFF} };

	/*
	Note on entity manager behaviour:
	On release builds registering a component twice, adding a component twice
	or removing a non-existent component results in undefined behaviour.
	In debug-mode asserts should catch this.
	*/
	class EntityManager {
	public:
		std::unique_ptr<SystemDispatch> systemDispatch_ = std::make_unique<SystemDispatch>();
		std::array<std::shared_ptr<ComponentPool>, MAX_COMPONENTS> componentPools_;

		static EntityManager* GetOrCreate();

		Entity Create();
		void Destroy(const Entity& entity);
		bool IsValid(const Entity& entity);

		template<typename C>
		void RegisterComponent() {
			size_t componentID = ComponentID::GetUnique<C>();
			assert(componentPools_[componentID] == nullptr && "Failed to register component: Component is already registered with the entity manager\n");
			componentPools_[componentID] = std::make_shared<SparseSet<C>>();
		}

		template<typename... C>
		void RegisterComponents() {
			(RegisterComponent<C>(), ...);
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
		C* const GetComponentPtr(const Entity& entity) const {
			return std::static_pointer_cast<SparseSet<C>>(componentPools_[ComponentID::GetUnique<C>()])->GetComponentPtr(entity.id_);
		}

		template<typename C>
		bool HasComponent(const Entity& entity) const {
			return std::static_pointer_cast<SparseSet<C>>(componentPools_[ComponentID::GetUnique<C>()])->HasComponent(entity.id_);
		}

		template<typename C>
		void RemoveComponent(const Entity& entity) const {
			size_t componentID = ComponentID::GetUnique<C>();
			assert(componentPools_[componentID] != nullptr && "Failed to remove component: Component isn't registered with the entity manager\n");
			std::static_pointer_cast<SparseSet<C>>(componentPools_[componentID])->Remove(entity.id_);
		}

		template<typename C>
		using sparseSetPtr = std::shared_ptr<SparseSet<C>>;

		template<typename... Components, typename F>
		void ForEach(F fun) {
			std::tuple pools{ GetPool<Components>()... };

			// Get the smallest pool to iterate from as a base
			std::vector<uint32_t> smallest = std::get<0>(pools)->entityFromComponentIndices_;
			ForEachTuple(pools, [&](auto&& value) {
				if(value->entityFromComponentIndices_.size() < smallest.size()) {
					smallest = value->entityFromComponentIndices_;
				}
			});

			for(auto entityIndex : smallest) {
				const bool allValid = ((std::get<sparseSetPtr<Components>>(pools)->sparse_[entityIndex] != 0xFFFF'FFFF) && ...);
				if(!allValid) continue;

				fun(entities_[sparse_[entityIndex]], std::get<sparseSetPtr<Components>>(pools)->dense_[std::get<sparseSetPtr<Components>>(pools)->sparse_[entityIndex]]...);

				(RemoveTag<Components>(entities_[sparse_[entityIndex]]), ...);
			}
		}

		template<typename... Components, typename F>
		void ParForEach(F fun) {
			std::tuple pools{ GetPool<Components>()... };

			// Get the smallest pool to iterate from as a base
			std::vector<uint32_t> smallest = std::get<0>(pools)->entityFromComponentIndices_;
			ForEachTuple(pools, [&](auto&& value) {
				if(value->entityFromComponentIndices_.size() < smallest.size()) {
					smallest = value->entityFromComponentIndices_;
				}
			});

			std::for_each(std::execution::par, smallest.begin(), smallest.end(), [&](auto entityIndex) {
				const bool allValid = ((std::get<sparseSetPtr<Components>>(pools)->sparse_[entityIndex] != 0xFFFF'FFFF) && ...);
				if(!allValid) return;

				fun(entities_[sparse_[entityIndex]], std::get<sparseSetPtr<Components>>(pools)->dense_[std::get<sparseSetPtr<Components>>(pools)->sparse_[entityIndex]]...);

				(RemoveTag<Components>(entities_[sparse_[entityIndex]]), ...);
			});
		}

		uint32_t GetEntityVersion(const EntityID id) {
			return entityVersions_[id.version];
		}

	private:
		uint32_t nextEntityId_ = 0;

		std::vector<uint32_t> sparse_ = std::vector<uint32_t>(INIT_MAX_ENTITIES, 0xFFFFFFFF);
		std::vector<uint32_t> entityVersions_ = std::vector<uint32_t>(INIT_MAX_ENTITIES, 1);
		std::vector<Entity> entities_;

		// Holds free entity indices
		std::deque<uint32_t> entityFreeList_;

		// Use default constructor/destructor
		EntityManager() = default;
		~EntityManager() = default;
		// Delete copy constructor and assignment operator
		EntityManager(const EntityManager&) = delete;
		EntityManager& operator= (const EntityManager&) = delete;

		template<typename C>
		std::shared_ptr<SparseSet<C>> GetPool() {
			size_t componentID = ComponentID::GetUnique<C>();
			assert(componentPools_[componentID] != nullptr && "Failed to get pool: Component isn't registered with the entity manager\n");
			return std::static_pointer_cast<SparseSet<C>>(componentPools_[componentID]);
		}

		template<typename... Components>
		std::vector<uint32_t>& SmallestPoolEntityFromComponentIndices() {
			std::size_t sizes[]{ GetPool<Components>()->dense_.size()... };
			std::vector<uint32_t>* vectors[]{ &GetPool<Components>()->entityFromComponentIndices_... };

			auto smallest_size_ptr = std::min_element(std::begin(sizes), std::end(sizes));
			return *vectors[smallest_size_ptr - sizes];
		}

		template<typename Tuple, typename F, std::size_t... I>
		constexpr F ForEachTupleImpl(Tuple&& t, F&& f, std::index_sequence<I...>) {
			return (void)std::initializer_list<int>{(std::forward<F>(f)(std::get<I>(std::forward<Tuple>(t))), 0)...}, f;
		}

		template<typename Tuple, typename F>
		constexpr F ForEachTuple(Tuple&& t, F&& f) {
			return ForEachTupleImpl(std::forward<Tuple>(t), std::forward<F>(f),
				std::make_index_sequence<std::tuple_size<std::remove_reference_t<Tuple>>::value>{});
		}

		template<typename C>
		void RemoveTag(Entity entity) {
			if constexpr(std::is_base_of_v<EventTagComponent, C>) {
				entity.RemoveComponent<C>();
			}
		}
	};

	template<typename T>
	inline void Entity::AddComponent() {
		manager_->AddComponent<T>(*this);
	}

	// Aggregate initialization
	template<typename T>
	inline void Entity::AddComponent(T&& t) {
		manager_->AddComponent<T>(*this, std::move(t));
	}

	template<typename... T>
	inline void Entity::AddComponents() {
		(manager_->AddComponent<T>(*this), ...);
	}

	template<typename T>
	inline T* const Entity::GetComponentPtr() {
		return manager_->GetComponentPtr<T>(*this);
	}

	template<typename T>
	bool Entity::HasComponent() {
		return manager_->HasComponent<T>(*this);
	}

	template<typename T>
	inline void Entity::RemoveComponent() {
		manager_->RemoveComponent<T>(*this);
	}
}