#pragma once

#include <stdint.h>
#include <glm/vec3.hpp>

namespace Quadbit {
	struct alignas(16) MeshMVP {
		glm::mat4 mvp;
	};

	struct Mesh {
		uint16_t vertexHandle;
		uint16_t indexHandle;
		MeshMVP position;
	};
}
