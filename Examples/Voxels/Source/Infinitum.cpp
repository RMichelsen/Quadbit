#include "Infinitum.h"

#include "Engine/Core/InputHandler.h"
#include "Engine/Entities/EntityManager.h"

#include "Data/Components.h"
#include "Systems/VoxelGenerationSystem.h"
#include "Systems/MeshGenerationSystem.h"
#include "Systems/ThirdPersonCameraSystem.h"
#include "Systems/PlayerMovementSystem.h"

void Infinitum::Init() {
	// Setup entities
	auto entityManager = Quadbit::EntityManager::GetOrCreate();
	entityManager->RegisterComponents<VoxelBlockComponent, VoxelBlockUpdateTag, MeshGenerationUpdateTag, MeshReadyTag, PlayerTag>();

	//camera_ = entityManager->Create();
	//camera_.AddComponent<Quadbit::RenderCamera>(Quadbit::RenderCamera(0.0f, 0.0f, glm::vec3(), renderer_->GetAspectRatio(), 10000.0f));
	//renderer_->RegisterCamera(camera_);

	renderMeshInstance_ = renderer_->CreateRenderMeshInstance({
			Quadbit::QbVkVertexInputAttribute::QBVK_VERTEX_ATTRIBUTE_POSITION,
			Quadbit::QbVkVertexInputAttribute::QBVK_VERTEX_ATTRIBUTE_COLOUR
		},
		"Resources/Shaders/Compiled/voxel_vert.spv", "main", "Resources/Shaders/Compiled/voxel_frag.spv", "main"
	);

	player_ = entityManager->Create();
	player_.AddComponent<Quadbit::RenderTransformComponent>(Quadbit::RenderTransformComponent(1.0f, glm::vec3(32.0f, 60.0f, 32.0f), glm::quat{ 0, 0, 0, 1 }));
	player_.AddComponent<Quadbit::RenderMeshComponent>(renderer_->CreateMesh(cubeVertices, sizeof(VoxelVertex), cubeIndices, renderMeshInstance_));
	player_.AddComponent<PlayerTag>();

	// Initialize the noise generator 
	fastnoise_ = FastNoiseSIMD::NewFastNoiseSIMD();
	fastnoise_->SetFrequency(0.032f);

	// Spawn base
	auto entity = entityManager->Create();
	entity.AddComponent<Quadbit::RenderTransformComponent>(Quadbit::RenderTransformComponent(1.0f, glm::vec3(0.0f, 0.0f, 0.0f), glm::quat()));
	entity.AddComponents<VoxelBlockComponent, VoxelBlockUpdateTag>();
}

void Infinitum::Simulate(float deltaTime) {
	static int i = 1;
	auto entityManager = Quadbit::EntityManager::GetOrCreate();

	auto t1 = std::chrono::high_resolution_clock::now();
	entityManager->systemDispatch_->RunSystem<VoxelGenerationSystem>(deltaTime, fastnoise_);
	entityManager->systemDispatch_->RunSystem<MeshGenerationSystem>(deltaTime, renderer_.get(), renderMeshInstance_);
	auto t2 = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
	if(duration > 100) {
		printf("Generated voxels in %I64i microseconds\n", duration);
	}

	if(Quadbit::InputHandler::KeyPressed(0x50)) {
		auto entity = entityManager->Create();
		entity.AddComponent<Quadbit::RenderTransformComponent>(Quadbit::RenderTransformComponent(1.0f, glm::vec3(i++ * VOXEL_BLOCK_WIDTH, 0.0f, 0.0f), glm::quat()));
		entity.AddComponents<VoxelBlockComponent, VoxelBlockUpdateTag>();
	}

}

void Infinitum::DrawFrame() {
	renderer_->DrawFrame();
}