#pragma once

#include "../../Entities/EntityManager.h"
#include "../../Rendering/RenderTypes.h"
#include "../../Physics/PhysicsComponents.h"
#include "../../Physics/PxCompat.h"

namespace Quadbit {
	struct PositionUpdateSystem : ComponentSystem {
		void Update(float dt) {
			EntityManager::GetOrCreate()->ForEach<RenderTransformComponent, CharacterControllerComponent>([&](auto& entity, RenderTransformComponent& objTransform, CharacterControllerComponent& character) {
				auto position = character.controller->getPosition();
				objTransform.SetPosition({ position.x, position.y, position.z });
				//objTransform.SetPosition(PxVec3ToGlm(dynamicBody.rigidBody->getGlobalPose().p));
				//objTransform.SetRotation(PxQuatToGlm(dynamicBody.rigidBody->getGlobalPose().q));
			});
		}
	};
}