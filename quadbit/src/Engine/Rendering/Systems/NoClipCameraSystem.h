#pragma once

#include <WinUser.h>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>

#include "Engine/Application/InputHandler.h"
#include "Engine/Rendering/RenderTypes.h"
#include "Engine/Entities/EntityManager.h"
#include "Engine/Entities/ComponentSystem.h"

namespace Quadbit {
	struct NoClipCameraSystem : ComponentSystem {
		void Init() {
			EntityManager::Instance().ForEach<RenderCamera>([&](Entity entity, RenderCamera& camera) {
				camera.front.x = cos(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
				camera.front.y = sin(glm::radians(camera.pitch));
				camera.front.z = sin(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
				camera.front = glm::normalize(camera.front);
				camera.view = glm::lookAt(camera.position, camera.position + camera.front, camera.up);
				});
		}

		void Update(float deltaTime) {
			auto& inputHandler = InputHandler::Instance();

			EntityManager::Instance().ForEach<RenderCamera>([&](Entity entity, RenderCamera& camera) {
				if (inputHandler.mouseDrag_.right.active && !camera.dragActive) {
					camera.dragActive = true;
					ShowCursor(false);
				}
				else if (!inputHandler.mouseDrag_.right.active && camera.dragActive) {
					camera.dragActive = false;
					// Set the cursor back to the cached position
					SetCursorPos(inputHandler.mouseDrag_.right.cachedPos.x, inputHandler.mouseDrag_.right.cachedPos.y);
					ShowCursor(true);
				}

				if (inputHandler.mouseDrag_.right.active || inputHandler.keyState_[0x41] || inputHandler.keyState_[0x44] ||
					inputHandler.keyState_[0x53] || inputHandler.keyState_[0x57]) {

					const float cameraSpeed = 100.0f * deltaTime;

					// 'W'
					if (inputHandler.keyState_[0x57]) {
						camera.position += camera.front * cameraSpeed;
					}
					// 'S'
					if (inputHandler.keyState_[0x53]) {
						camera.position -= camera.front * cameraSpeed;
					}
					// 'D'
					if (inputHandler.keyState_[0x44]) {
						camera.position += glm::normalize(glm::cross(camera.front, glm::vec3(0.0f, 1.0f, 0.0f))) * cameraSpeed;
					}
					// 'A'
					if (inputHandler.keyState_[0x41]) {
						camera.position -= glm::normalize(glm::cross(camera.front, glm::vec3(0.0f, 1.0f, 0.0f))) * cameraSpeed;
					}

					if (inputHandler.mouseDrag_.right.active) {
						const float dragSpeed = 100.0f * deltaTime;

						if (inputHandler.mouseDelta_.x != 0) {
							camera.yaw += static_cast<float>(inputHandler.mouseDelta_.x)* dragSpeed;
						}
						if (inputHandler.mouseDelta_.y != 0) {
							camera.pitch -= static_cast<float>(inputHandler.mouseDelta_.y)* dragSpeed;
							// Put constraints on pitch.
							// 1.5533rad is approx 89deg
							if (camera.pitch > 89.0f) {
								camera.pitch = 89.0f;
							}
							if (camera.pitch < -89.0f) {
								camera.pitch = -89.0f;
							}
						}
						// Update camera front (direction)
						camera.front.x = cos(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
						camera.front.y = sin(glm::radians(camera.pitch));
						camera.front.z = sin(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
						camera.front = glm::normalize(camera.front);
					}
					// Update view matrix
					camera.view = glm::lookAt(camera.position, camera.position + camera.front, camera.up);
				}
				});
		}
	};
}
