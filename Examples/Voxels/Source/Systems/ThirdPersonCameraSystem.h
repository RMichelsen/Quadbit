#pragma once

#include <WinUser.h>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>

#include "Engine/Core/InputHandler.h"
#include "Engine/Entities/EntityManager.h"
#include "Engine/Entities/ComponentSystem.h"
#include "Engine/Rendering/RenderTypes.h"

#include "../Data/Components.h"

using namespace Quadbit;

struct ThirdPersonCameraSystem : ComponentSystem {
	void Update(float dt, Entity objectToFollow) {
		//auto transform = objectToFollow.GetComponentPtr<RenderTransformComponent>();

		//EntityManager::GetOrCreate()->ForEach<RenderCamera>([&](Entity entity, RenderCamera& camera) {
		//	if(InputHandler::rightMouseDragging && !camera.dragActive) {
		//		camera.dragActive = true;
		//		ShowCursor(false);
		//	}
		//	else if(!InputHandler::rightMouseDragging && camera.dragActive) {
		//		camera.dragActive = false;
		//		// Set the cursor back to the cached position
		//		SetCursorPos(InputHandler::rightDragCachedPos.x, InputHandler::rightDragCachedPos.y);
		//		ShowCursor(true);
		//	}

		//	const float cameraSpeed = CAMERA_MOVE_SPEED * dt;

		//	if(InputHandler::rightMouseDragging) {
		//		const float dragSpeed = CAMERA_DRAG_SPEED * dt;

		//		if(InputHandler::deltaMouseMovement[1] != 0) {
		//			camera.pitch += static_cast<float>(InputHandler::deltaMouseMovement[1]) * dragSpeed;
		//		}
		//	}

		//	camera.front = glm::normalize(transform->rotation * glm::vec3(1.0f, 0.5f + camera.pitch, 0.0f));
		//	camera.position = transform->position - (CAMERA_FOLLOW_DISTANCE * camera.front);
		//	camera.view = glm::lookAt(camera.position, transform->position, camera.up);
		//});
	}
};
