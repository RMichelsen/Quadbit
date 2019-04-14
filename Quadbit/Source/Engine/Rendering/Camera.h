#pragma once
#include <glm/mat4x4.hpp>

namespace Camera {
	bool IsMoving();
	void UpdateCamera();
	glm::mat4 GetPerspectiveMatrix();
	glm::mat4 GetViewMatrix();
}