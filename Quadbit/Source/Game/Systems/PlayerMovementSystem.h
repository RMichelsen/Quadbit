#pragma once

#include <WinUser.h>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>

#include "../../Engine/Core/InputHandler.h"
#include "../../Engine/Entities/EntityManager.h"
#include "../../Engine/Entities/ComponentSystem.h"
#include "../../Engine/Rendering/RenderTypes.h"
#include "../../Engine/Physics/PhysicsComponents.h"
#include "../../Engine/Physics/PxCompat.h"

#include "../Data/Components.h"

using namespace Quadbit;

struct PlayerMovementSystem : ComponentSystem {
	void Update(float dt) {
		auto entityManager = EntityManager::GetOrCreate();
		entityManager->ForEach<RenderTransformComponent, CharacterControllerComponent, PlayerTag>
			([&](Entity& entity, RenderTransformComponent& transform, CharacterControllerComponent& character, auto& tag) {

			const float movementSpeed = 50.0f * dt;

			PxVec3 displacement(0.0f, -9.81f * 5.0f * dt, 0.0f);

			// 'W'
			if(InputHandler::keycodes[0x57]) {
				displacement += GlmVec3ToPx(transform.rotation * glm::vec3(movementSpeed, 0.0f, 0.0f));
				//character.controller->move(GlmVec3ToPx(transform.rotation * glm::vec3(movementSpeed, 0.0f, 0.0f)), 0.01f, dt, nullptr);
			}
			// 'S'
			if(InputHandler::keycodes[0x53]) {
				displacement += GlmVec3ToPx(transform.rotation * glm::vec3(-movementSpeed, 0.0f, 0.0f));
				//character.controller->move(GlmVec3ToPx(transform.rotation * glm::vec3(-movementSpeed, 0.0f, 0.0f)), 0.01f, dt, nullptr);
			}
			// 'A'
			if(InputHandler::keycodes[0x41]) {
				displacement += GlmVec3ToPx(transform.rotation * glm::vec3(0.0f, 0.0f, movementSpeed));
				//character.controller->move(GlmVec3ToPx(transform.rotation * glm::vec3(0.0f, 0.0f, movementSpeed)), 0.01f, dt, nullptr);
			}
			// 'D'
			if(InputHandler::keycodes[0x44]) {
				displacement += GlmVec3ToPx(transform.rotation * glm::vec3(0.0f, 0.0f, -movementSpeed));
				//character.controller->move(GlmVec3ToPx(transform.rotation * glm::vec3(0.0f, 0.0f, -movementSpeed)), 0.01f, dt, nullptr);
			}

			character.controller->move(displacement, 0.0001f, dt, PxControllerFilters());

			if(InputHandler::rightMouseDragging) {
				const float dragSpeed = 5.0f * dt;
				
				if(InputHandler::deltaMouseMovement[0] != 0) {
					transform.UpdateRotation(static_cast<float>(InputHandler::deltaMouseMovement[0]) * dragSpeed, glm::vec3(0.0f, 1.0f, 0.0f));
				}
			}
		});
	}
};