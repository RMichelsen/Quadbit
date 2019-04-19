#pragma once

namespace Quadbit::Camera {
	bool IsMoving();
	void UpdateCamera(float deltaTime);
	glm::mat4 GetPerspectiveMatrix();
	glm::mat4 GetViewMatrix();
}