#pragma once
#include <cstddef>
#include <cstdint>

namespace Quadbit {
	constexpr size_t MAX_SYSTEMS = 256;
	constexpr size_t MAX_COMPONENTS = 128;
	constexpr size_t INIT_MAX_ENTITIES = 5'000'000;

	class SystemID {
	public:
		template<typename>
		static std::size_t GetUnique() noexcept {
			static const std::size_t val = GetID();
			return val;
		}
	private:
		static std::size_t GetID() noexcept {
			static std::size_t val = 0;
			return val++;
		}
	};

	class ComponentID {
	public:
		template<typename>
		static std::size_t GetUnique() noexcept {
			static const std::size_t val = GetID();
			return val;
		}
	private:
		static std::size_t GetID() noexcept {
			static std::size_t val = 0;
			return val++;
		}
	};

	struct EntityID {
		uint32_t index : 24;
		uint32_t version : 8;

		EntityID() : index(0), version(0) {}
		EntityID(uint32_t id, uint32_t version) : index(id), version(version) {};

		bool operator == (const EntityID& other) const { return index == other.index; }
		bool operator != (const EntityID& other) const { return index != other.index; }
	};

	class ComponentPool {
	public:
		virtual void RemoveIfExists(EntityID id) = 0;
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

		//using has_cleanup = decltype(std::declval<T>().Cleanup());
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

			//if constexpr(SFINAE::is_detected_v<has_cleanup, T>) {
			//	dense_.back()->Cleanup();
			//}

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

			//if constexpr(SFINAE::is_detected_v<has_cleanup, T>) {
			//	dense_.back()->Cleanup();
			//}

			dense_.pop_back();
			entityFromComponentIndices_.pop_back();
		}

		bool HasComponent(EntityID id) {
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