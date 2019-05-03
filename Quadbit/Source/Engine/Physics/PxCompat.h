#pragma once

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <PxPhysicsAPI.h>

namespace Quadbit {
	inline PxVec3 GlmVec3ToPx(const glm::vec3& vec) {
		return { vec.x, vec.y, vec.z };
	}
	inline glm::vec3 PxVec3ToGlm(const PxVec3& vec) {
		return { vec.x, vec.y, vec.z };
	}
	inline PxQuat GlmQuatToPx(const glm::quat& quat) {
		return { quat.x, quat.y, quat.z, quat.w };
	}
	inline glm::quat PxQuatToGlm(const PxQuat& quat) {
		return { quat.w, quat.x, quat.y, quat.z };
	}
}