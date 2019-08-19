#pragma once

#include <glm/glm.hpp>
#include "../Data/Components.h"

namespace CommonUtils {
	// Converts (flattens) a 3D index
	inline uint32_t ConvertIndex(glm::int3 index, glm::int3 extents) {
		return index.x + extents.x * (index.z + extents.z * index.y);
	}
}