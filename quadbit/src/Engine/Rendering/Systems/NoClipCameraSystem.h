	#pragma once

#include <WinUser.h>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>

#include "Engine/Core/InputHandler.h"
#include "Engine/Core/QbRenderDefs.h"
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
			EntityManager::Instance().ForEach<RenderCamera>([&](Entity entity, RenderCamera& camera) {
				if(InputHandler::rightMouseDragging && !camera.dragActive) {
					camera.dragActive = true;
					ShowCursor(false);
				}
				else if(!InputHandler::rightMouseDragging && camera.dragActive) {
					camera.dragActive = false;
					// Set the cursor back to the cached position
					SetCursorPos(InputHandler::rightDragCachedPos.x, InputHandler::rightDragCachedPos.y);
					ShowCursor(true);
				}

				if(InputHandler::rightMouseDragging || InputHandler::keycodes[0x41] || InputHandler::keycodes[0x44] ||
					InputHandler::keycodes[0x53] || InputHandler::keycodes[0x57]) {

					const float cameraSpeed = 100.0f * deltaTime;

					// 'W'
					if(InputHandler::keycodes[0x57]) {
						camera.position += camera.front * cameraSpeed;
					}
					// 'S'
					if(InputHandler::keycodes[0x53]) {
						camera.position -= camera.front * cameraSpeed;
					}
					// 'D'
					if(InputHandler::keycodes[0x44]) {
						camera.position += glm::normalize(glm::cross(camera.front, glm::vec3(0.0f, 1.0f, 0.0f))) * cameraSpeed;
					}
					// 'A'
					if(InputHandler::keycodes[0x41]) {
						camera.position -= glm::normalize(glm::cross(camera.front, glm::vec3(0.0f, 1.0f, 0.0f))) * cameraSpeed;
					}

					if(InputHandler::rightMouseDragging) {
						const float dragSpeed = 100.0f * deltaTime;

						if(InputHandler::deltaMouseMovement[0] != 0) {
							camera.yaw += static_cast<float>(InputHandler::deltaMouseMovement[0]) * dragSpeed;
						}
						if(InputHandler::deltaMouseMovement[1] != 0) {
							camera.pitch -= static_cast<float>(InputHandler::deltaMouseMovement[1]) * dragSpeed;
							// Put constraints on pitch.
							// 1.5533rad is approx 89deg
							if(camera.pitch > 89.0f) {
								camera.pitch = 89.0f;
							}
							if(camera.pitch < -89.0f) {
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
