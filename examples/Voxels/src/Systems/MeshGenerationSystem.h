#pragma once

#include "../Data/Defines.h"
#include "../Data/Components.h"
#include "../Utils/TerrainTools.h"
#include "../Utils/VoxelMesher.h"

#include "Engine/Rendering/Renderer.h"
#include "Engine/Entities/EntityManager.h"


struct MeshGenerationSystem : Quadbit::ComponentSystem {
	void GreedyMeshGeneration(Quadbit::QbVkRenderer* renderer, const Quadbit::QbVkRenderMeshInstance* rMeshInstance) {
		auto& entityManager = Quadbit::EntityManager::Instance();

		entityManager.ParForEachAddTag<Quadbit::RenderTransformComponent, VoxelBlockComponent, MeshGenerationUpdateTag>
			([&](Quadbit::Entity entity, Quadbit::RenderTransformComponent& transform, VoxelBlockComponent& block, auto& tag) {

			VoxelMesher::GreedyMeshVoxelBlock(block.voxels, block.vertices, block.indices, { VOXEL_BLOCK_WIDTH, VOXEL_BLOCK_WIDTH, VOXEL_BLOCK_WIDTH });

		}, MeshReadyTag{});

		entityManager.ForEach<VoxelBlockComponent, MeshReadyTag>
			([&](Quadbit::Entity entity, VoxelBlockComponent& block, auto& tag) {
			entity.AddComponent<Quadbit::RenderMeshComponent>(renderer->CreateMesh(block.vertices, sizeof(VoxelVertex), block.indices, rMeshInstance));
		});
	}

	void CulledMeshGeneration(Quadbit::QbVkRenderer* renderer, const Quadbit::QbVkRenderMeshInstance* rMeshInstance) {
		auto& entityManager = Quadbit::EntityManager::Instance();

		entityManager.ParForEachAddTag<Quadbit::RenderTransformComponent, VoxelBlockComponent, MeshGenerationUpdateTag>
			([&](Quadbit::Entity entity, Quadbit::RenderTransformComponent& transform, VoxelBlockComponent& block, auto& tag) {

			VoxelMesher::CulledMeshVoxelBlock(block.voxels, block.vertices, block.indices, { VOXEL_BLOCK_WIDTH, VOXEL_BLOCK_WIDTH, VOXEL_BLOCK_WIDTH });

		}, MeshReadyTag{});

		entityManager.ForEach<VoxelBlockComponent, MeshReadyTag>
			([&](Quadbit::Entity entity, VoxelBlockComponent& block, auto& tag) {
			entity.AddComponent<Quadbit::RenderMeshComponent>(renderer->CreateMesh(block.vertices, sizeof(VoxelVertex), block.indices, rMeshInstance));
		});

	}

	void Update(float dt, Quadbit::QbVkRenderer* renderer, const Quadbit::QbVkRenderMeshInstance* rMeshInstance) {
		GreedyMeshGeneration(renderer, rMeshInstance);
	}
};
