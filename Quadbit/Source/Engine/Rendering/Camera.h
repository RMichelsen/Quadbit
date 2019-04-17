#pragma once
#include <glm/mat4x4.hpp>

namespace Quadbit::Camera {
	bool IsMoving();
	void UpdateCamera(float deltaTime);
	glm::mat4 GetPerspectiveMatrix();
	glm::mat4 GetViewMatrix();
}