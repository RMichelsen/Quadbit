#include <PCH.h>
#include "Infinitum.h"

#include "../Engine/Core/InputHandler.h"
#include "../Engine/Entities/EntityManager.h"

#include "Data/Components.h"
#include "Systems/VoxelGenerationSystem.h"
#include "Systems/MeshGenerationSystem.h"
#include "Systems/ThirdPersonCameraSystem.h"
#include "Systems/PlayerMovementSystem.h"


void Infinitum::Init() {
	// Initialize the physics world
	physicsWorld_ = Quadbit::PhysicsWorld::GetOrCreate();

	// Setup entities
	auto entityManager = Quadbit::EntityManager::GetOrCreate();
	entityManager->RegisterComponents<VoxelBlockComponent, VoxelBlockUpdateTag, MeshGenerationUpdateTag>();
	entityManager->RegisterComponent<PlayerTag>();

	camera_ = entityManager->Create();
	camera_.AddComponent<Quadbit::RenderCamera>(Quadbit::RenderCamera(0.0f, 0.0f, glm::vec3(), renderer_->GetAspectRatio(), 10000.0f));
	renderer_->RegisterCamera(camera_);

	player_ = entityManager->Create();
	player_.AddComponent<Quadbit::TransformComponent>(Quadbit::TransformComponent(1.0f, glm::vec3(32.0f, 40.0f, 32.0f), glm::quat{ 0, 0, 0, 1 }));
	player_.AddComponent<Quadbit::RenderMeshComponent>(renderer_->CreateMesh(cubeVertices, cubeIndices));
	player_.AddComponent<PlayerTag>();

	// Initialize the noise generator 
	fastnoise_ = FastNoiseSIMD::NewFastNoiseSIMD();
	fastnoise_->SetFrequency(0.032f);
}

void Infinitum::Simulate(float deltaTime) {
	static int i = 0;
	auto entityManager = Quadbit::EntityManager::GetOrCreate();


	if(Quadbit::InputHandler::keycodes[0x50]) {
		auto entity = entityManager->Create();
		entity.AddComponent<Quadbit::TransformComponent>(Quadbit::TransformComponent(1.0f, glm::vec3(i++ * VOXEL_BLOCK_WIDTH, 0.0f, 0.0f), glm::quat()));
		entity.AddComponents<VoxelBlockComponent, VoxelBlockUpdateTag>();
	}

	entityManager->systemDispatch_->RunSystem<VoxelGenerationSystem>(deltaTime, fastnoise_);
	entityManager->systemDispatch_->RunSystem<MeshGenerationSystem>(deltaTime, renderer_.get());
	entityManager->systemDispatch_->RunSystem<PlayerMovementSystem>(deltaTime);
	entityManager->systemDispatch_->RunSystem<ThirdPersonCameraSystem>(deltaTime, player_);
}

void Infinitum::DrawFrame() {
	renderer_->DrawFrame();
}