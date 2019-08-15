#pragma once

#include "Extern/FastNoiseSIMD/FastNoiseSIMD.h"

#include "../Data/Components.h"

namespace TerrainTools {
	static Region GetRegion(float selector) {
		return Region::Forest;
	}

	static glm::vec3 GetColour(Region region, uint32_t height, float colourCoeff) {
		if(region == Region::Forest) {
			const float heightCoeff = static_cast<float>(height) / VOXEL_BLOCK_WIDTH;

			glm::vec3 greenyellow = glm::vec3(0.68f, 1.0f, 0.18f);
			glm::vec3 springgreen = glm::vec3(0.0f, 1.0f, 0.50f);

			return glm::lerp(greenyellow, springgreen, colourCoeff);
		}
		else return glm::vec3(1.0f, 1.0f, 1.0f);
	}
}
