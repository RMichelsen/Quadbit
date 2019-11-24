#include "Testing.h"

void Testing::Init() {
	auto entity = entityManager_->Create();
	entityManager_->AddComponent<Quadbit::PBRSceneComponent>(entity, graphics_->LoadPBRModel("resources/Sponza/Sponza.gltf"));
	entityManager_->AddComponent<Quadbit::RenderTransformComponent>(entity, 
		Quadbit::RenderTransformComponent(10.0f, { 0.0f, 0.0f, 0.0f }, glm::quat(1, 0, 0, 0)));
}

void Testing::Simulate(float deltaTime) {

}