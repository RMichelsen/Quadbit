#pragma once
#include <vector>

#include "../Entities/EntitiesCommon.h"

namespace Quadbit {

	class ComponentPool {
	public:
		virtual void RemoveIfExists(EntityID id) = 0;
		virtual bool HasComponent(EntityID id) = 0;
	};

	template<typename T>
	class SparseSet : public ComponentPool {
	public:
		std::vector<uint32_t> sparse_ = std::vector<uint32_t>(INIT_MAX_ENTITIES, 0xFFFF'FFFF);
		std::vector<T> dense_;
		std::vector<uint32_t> entityFromComponentIndices_;

		SparseSet() {
			QB_LOG_INFO("Sparse of type \"%s\" (%zi bytes) created\n", typeid(T).name(), sizeof(T));
		}

		void Insert(EntityID id) {
			assert(id.index < sparse_.size());
			assert(sparse_[id.index] == 0xFFFF'FFFF && "Failed to add component: Component is already part of entity");

			sparse_[id.index] = static_cast<uint32_t>(dense_.size());
			dense_.push_back(T());
			entityFromComponentIndices_.push_back(id.index);
		}

		void Insert(EntityID id, T&& t) {
			assert(id.index < sparse_.size());
			assert(sparse_[id.index] == 0xFFFF'FFFF && "Failed to add component: Component is already part of entity");

			sparse_[id.index] = static_cast<uint32_t>(dense_.size());
			dense_.push_back(t);
			entityFromComponentIndices_.push_back(id.index);
		}

		void Remove(EntityID id) {
			assert(id.index < sparse_.size());
			assert(sparse_[id.index] != 0xFFFF'FFFF && "Failed to remove component: Component is not part of the entity");

			// Removal works by swap and pop
			uint32_t lastIndex = FindEntityID(static_cast<uint32_t>(dense_.size()) - 1);
			std::swap(dense_[sparse_[id.index]], dense_[sparse_[lastIndex]]);
			std::swap(entityFromComponentIndices_[sparse_[id.index]], entityFromComponentIndices_[sparse_[lastIndex]]);
			sparse_[lastIndex] = sparse_[id.index];
			// id.index is now FREE to be used (add to free list)
			sparse_[id.index] = 0xFFFFFFFF;
			dense_.pop_back();
			entityFromComponentIndices_.pop_back();
		}

		void RemoveIfExists(EntityID id) override {
			assert(id.index < sparse_.size());
			if(sparse_[id.index] == 0xFFFF'FFFF) return;

			// Removal works by swap and pop
			uint32_t lastIndex = FindEntityID(static_cast<uint32_t>(dense_.size()) - 1);
			std::swap(dense_[sparse_[id.index]], dense_[sparse_[lastIndex]]);
			std::swap(entityFromComponentIndices_[sparse_[id.index]], entityFromComponentIndices_[sparse_[lastIndex]]);
			sparse_[lastIndex] = sparse_[id.index];
			// id.index is now FREE to be used (add to free list)
			sparse_[id.index] = 0xFFFFFFFF;
			dense_.pop_back();
			entityFromComponentIndices_.pop_back();
		}

		bool HasComponent(EntityID id) override {
			assert(id.index < sparse_.size());
			return sparse_[id.index] != 0xFFFF'FFFF;
		}

		T* const GetComponentPtr(EntityID id) {
			assert(id.index < sparse_.size());
			assert(sparse_[id.index] != 0xFFFF'FFFF && "Failed to get component: Component is not part of the entity");

			return &dense_[sparse_[id.index]];
		}

		uint32_t FindEntityID(uint32_t denseIndex) {
			assert(denseIndex < entityFromComponentIndices_.size());

			return entityFromComponentIndices_[denseIndex];
		}

		T* GetRawPtr() {
			return dense_.data();
		}
	};
}