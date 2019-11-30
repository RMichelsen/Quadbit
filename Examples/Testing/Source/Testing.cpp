#include "Testing.h"

#include <cstdlib>
#include <ctime>

void Testing::Init() {
	Quadbit::PBRSceneComponent sponza = graphics_->LoadPBRModel("Assets/Testing/Models/DamagedHelmet/DamagedHelmet.glb");

	auto entity = entityManager_->Create();
	entityManager_->AddComponent<Quadbit::PBRSceneComponent>(entity, sponza);
	entityManager_->AddComponent<Quadbit::RenderTransformComponent>(entity,
		Quadbit::RenderTransformComponent(50.0f, { 0.0f, 0.0f, 0.0f }, glm::quat(1, 0, 0, 0)));
}

void Testing::Simulate(float deltaTime) {

}