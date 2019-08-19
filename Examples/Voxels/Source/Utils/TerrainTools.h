#pragma once

#include "Extern/FastNoiseSIMD/FastNoiseSIMD.h"

#include "../Data/Components.h"
#include "MagicaVoxImporter.h"

namespace TerrainTools {
	inline Region GetRegion(float selector) {
		return Region::Forest;
	}

	inline glm::float3 GetColour(Region region, uint32_t height, glm::int3 extents, float colourCoeff) {
		if(region == Region::Forest) {
			const float heightCoeff = static_cast<float>(height) / extents.y;

			glm::float3 greenyellow = glm::vec3(0.68f, 1.0f, 0.18f);
			glm::float3 springgreen = glm::vec3(0.0f, 1.0f, 0.50f);

			return glm::lerp(greenyellow, springgreen, colourCoeff);
		}
		else return glm::vec3(1.0f, 1.0f, 1.0f);
	}
}
