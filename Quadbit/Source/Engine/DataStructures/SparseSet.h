#pragma once

namespace Quadbit {
	constexpr size_t INIT_MAX_ENTITIES = 10000;

	template <typename T>
	class SparseSet {
	public:
		SparseSet() {
			dense_.reserve(100);

			sparse_.reserve(INIT_MAX_ENTITIES);
			componentIndices_.reserve(INIT_MAX_ENTITIES);
			QB_LOG_INFO("Sparse set of size %zi was created %s\n", sizeof(T), typeid(T).name());
			QB_LOG_INFO("Sparse set dense vector capacity %zi\n", dense_.capacity());
		}

		std::vector<uint32_t> sparse_;
		std::vector<T> dense_;
		std::vector<uint32_t> componentIndices_;
	};
}