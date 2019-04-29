#pragma once

#include <WinUser.h>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>

#include "../../Engine/Core/InputHandler.h"
#include "../../Engine/Entities/EntityManager.h"
#include "../../Engine/Entities/ComponentSystem.h"
#include "../../Engine/Rendering/RenderTypes.h"

#include "../Data/Components.h"

using namespace Quadbit;

struct PlayerMovementSystem : ComponentSystem {
	void Update(float dt) {
		auto entityManager = EntityManager::GetOrCreate();
		entityManager->ForEach<TransformComponent, PlayerTag>([&](Entity& entity, TransformComponent& transform, auto& tag) {

			const float movementSpeed = 50.0f;

			// 'W'
			if(InputHandler::keycodes[0x57]) {
				transform.UpdatePosition(transform.rotation * glm::vec3(movementSpeed * dt, 0.0f, 0.0f));
			}
			// 'S'
			if(InputHandler::keycodes[0x53]) {
				transform.UpdatePosition(transform.rotation * glm::vec3(-movementSpeed * dt, 0.0f, 0.0f));
			}
			// 'A'
			if(InputHandler::keycodes[0x41]) {
				transform.UpdatePosition(transform.rotation * glm::vec3(0.0f, 0.0f, movementSpeed * dt));
			}
			// 'D'
			if(InputHandler::keycodes[0x44]) {
				transform.UpdatePosition(transform.rotation * glm::vec3(0.0f, 0.0f, -movementSpeed * dt));
			}


			if(InputHandler::rightMouseDragging) {
				const float dragSpeed = 5.0f * dt;
				
				if(InputHandler::deltaMouseMovement[0] != 0) {
					transform.UpdateRotation(static_cast<float>(InputHandler::deltaMouseMovement[0]) * dragSpeed, glm::vec3(0.0f, 1.0f, 0.0f));
				}
			}
		});
	}
};