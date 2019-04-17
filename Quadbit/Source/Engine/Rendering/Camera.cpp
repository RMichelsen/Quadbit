#include <PCH.h>
#include "Camera.h"

#include "../Application/InputHandler.h"
#include "../Application/Time.h"

namespace Quadbit::Camera {
	// Anonymous namespace used for camera data
	namespace {
		glm::vec3 cameraPos = glm::vec3(0.0f, 4.0f, 6.0f);
		glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
		glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
		float pitch = -30.0f;
		float yaw = 270.0f;
		float roll = 0.0f;

		glm::mat4 view;
		// Horizontal FOV is 45, this is equal to 90 FOV vertically
		glm::mat4 perspective = glm::perspective(glm::radians(45.0f), 16 / (float)9, 0.1f, 1000.0f);

		void UpdateViewMatrix() {
			cameraFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
			cameraFront.y = sin(glm::radians(pitch));
			cameraFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
			cameraFront = glm::normalize(cameraFront);
			view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
		}

		bool dragActive = false;
	}

	bool IsMoving() {
		return InputHandler::rightMouseDragging ||
			InputHandler::keycodes[0x41] || InputHandler::keycodes[0x44] ||
			InputHandler::keycodes[0x53] || InputHandler::keycodes[0x57];
	}

	void UpdateCamera(float deltaTime) {
		if(InputHandler::rightMouseDragging && !dragActive) {
			dragActive = true;
			ShowCursor(false);
		}
		else if(!InputHandler::rightMouseDragging && dragActive) {
			dragActive = false;
			// Set the cursor back to the cached position
			SetCursorPos(InputHandler::rightDragCachedPos.x, InputHandler::rightDragCachedPos.y);
			ShowCursor(true);
		}

		if(IsMoving()) {
			float cameraSpeed = 10.0f * deltaTime;
			float dragSpeed = 200.0f * deltaTime;

			auto deltaMove = InputHandler::ProcessDeltaMovement();

			if(deltaMove[0] != 0) {
				yaw += static_cast<float>(deltaMove[0]) * dragSpeed;
			}
			if(deltaMove[1] != 0) {
				pitch -= static_cast<float>(deltaMove[1]) * dragSpeed;
				// Put constraints on pitch.
				// 1.5533rad is approx 89deg
				if(pitch > 89.0f) {
					pitch = 89.0f;
				}
				if(pitch < -89.0f) {
					pitch = -89.0f;
				}
			}

			// 'W'
			if(InputHandler::keycodes[0x57]) {
				cameraPos += cameraFront * cameraSpeed;
			}
			// 'S'
			if(InputHandler::keycodes[0x53]) {
				cameraPos -= cameraFront * cameraSpeed;
			}
			// 'D'
			if(InputHandler::keycodes[0x44]) {
				cameraPos += glm::normalize(glm::cross(cameraFront, glm::vec3(0.0f, 1.0f, 0.0f))) * cameraSpeed;
			}
			// 'A'
			if(InputHandler::keycodes[0x41]) {
				cameraPos -= glm::normalize(glm::cross(cameraFront, glm::vec3(0.0f, 1.0f, 0.0f))) * cameraSpeed;
			}
		}
		UpdateViewMatrix();
	}

	glm::mat4 GetPerspectiveMatrix() {
		return perspective;
	}

	glm::mat4 GetViewMatrix() {
		return view;
	}
}
