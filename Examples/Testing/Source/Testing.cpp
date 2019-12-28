#include "Testing.h"

#include <cstdlib>
#include <ctime>

void Testing::Init() {
	Quadbit::PBRSceneComponent scene = graphics_->LoadPBRScene("Assets/Testing/Models/corner.glb");
	auto entity = entityManager_->Create();
	entityManager_->AddComponent<Quadbit::PBRSceneComponent>(entity, scene);
	entityManager_->AddComponent<Quadbit::RenderTransformComponent>(entity,
		Quadbit::RenderTransformComponent(50.0f, { 0.0f, 0.0f, 0.0f }, glm::quat(1, 0, 0, 0)));
}

void Testing::Simulate(float deltaTime) {
}