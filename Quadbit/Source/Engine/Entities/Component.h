#pragma once

#include "../DataStructures/SparseSet.h"

namespace Quadbit {
	constexpr size_t MAX_COMPONENTS = 64;

	class ComponentID {
	public:
		template<typename>
		static std::size_t GetUnique() {
			static const std::size_t id = GetID();
			return id;
		}

	private:
		static std::size_t GetID() {
			static std::size_t id_counter = 0;
			assert(id_counter + 1 < MAX_COMPONENTS);
			return id_counter++;
		}
	};

	struct Component {
	};
}