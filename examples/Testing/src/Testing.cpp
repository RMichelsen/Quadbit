#include "Testing.h"

#include <cstdlib>
#include <ctime>

void Testing::Init() {
	Quadbit::PBRSceneComponent avocado = graphics_->LoadPBRModel("resources/Avocado/Avocado.glb");

	auto entity = entityManager_->Create();
	entityManager_->AddComponent<Quadbit::PBRSceneComponent>(entity, avocado);
	entityManager_->AddComponent<Quadbit::RenderTransformComponent>(entity,
		Quadbit::RenderTransformComponent(2000.0f, { 0.0f, 0.0f, 0.0f }, glm::quat(1, 0, 0, 0)));

	//for (int i = 0; i < 20; i++) {
	//	for (int j = 0; j < 20; j++) {
	//		for (int k = 0; k < 20; k++) {
	//			auto entity = entityManager_->Create();
	//			entityManager_->AddComponent<Quadbit::PBRSceneComponent>(entity, avocado);
	//			entityManager_->AddComponent<Quadbit::RenderTransformComponent>(entity,
	//				Quadbit::RenderTransformComponent(200.0f, { i * 20.0f, j * 20.0f, k * 20.0f }, glm::quat(1, 0, 0, 0)));
	//		}
	//	}
	//}

}

void Testing::Simulate(float deltaTime) {
}