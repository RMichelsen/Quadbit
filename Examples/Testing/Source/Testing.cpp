#include "Testing.h"

#include <cstdlib>
#include <ctime>

void Testing::Init() {
	Quadbit::PBRSceneComponent scene = graphics_->LoadPBRModel("Assets/Testing/Models/corner.glb");
	//Quadbit::PBRSceneComponent scene = graphics_->LoadPBRModel("Assets/Testing/Models/Sponza/sponza.gltf");
	//Quadbit::PBRSceneComponent scene = graphics_->LoadPBRModel("Assets/Testing/Models/DamagedHelmet/DamagedHelmet.glb");
	//Quadbit::PBRSceneComponent scene = graphics_->LoadPBRModel("Assets/Testing/Models/VC/VC.glb");

	for (int i = 0; i < 5; i++) {
		for (int j = 0; j < 5; j++) {
			auto entity = entityManager_->Create();
			entityManager_->AddComponent<Quadbit::PBRSceneComponent>(entity, scene);
			entityManager_->AddComponent<Quadbit::RenderTransformComponent>(entity,
				Quadbit::RenderTransformComponent(5.0f, { 5.0f * i * scene.extents.x, 0.0f, 5.0f * j * scene.extents.z }, glm::quat(1, 0, 0, 0)));
		}
	}

	//auto entity = entityManager_->Create();
	//entityManager_->AddComponent<Quadbit::PBRSceneComponent>(entity, scene);
	//entityManager_->AddComponent<Quadbit::RenderTransformComponent>(entity,
	//	Quadbit::RenderTransformComponent(1.0f, { 0.0f, 0.0f, 0.0f }, glm::quat(1, 0, 0, 0)));

	//entity = entityManager_->Create();
	//entityManager_->AddComponent<Quadbit::PBRSceneComponent>(entity, scene);
	//entityManager_->AddComponent<Quadbit::RenderTransformComponent>(entity,
	//	Quadbit::RenderTransformComponent(2.0f, { 0.0f, 10.0f, 0.0f }, glm::quat(1, 0, 0, 0)));
}

void Testing::Simulate(float deltaTime) {
}