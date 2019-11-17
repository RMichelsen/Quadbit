#pragma once

#include "../Data/Defines.h"
#include "../Data/Components.h"
#include "../Utils/TerrainTools.h"
#include "../Utils/VoxelMesher.h"

#include "Engine/API/Graphics.h"
#include "Engine/Entities/EntityManager.h"


struct MeshGenerationSystem : Quadbit::ComponentSystem {
	void GreedyMeshGeneration(Quadbit::Graphics* graphics, const Quadbit::QbVkRenderMeshInstance* rMeshInstance) {
		entityManager_->ParForEachAddTag<Quadbit::RenderTransformComponent, VoxelBlockComponent, MeshGenerationUpdateTag>
			([&](Quadbit::Entity entity, Quadbit::RenderTransformComponent& transform, VoxelBlockComponent& block, auto& tag) {

			VoxelMesher::GreedyMeshVoxelBlock(block.voxels, block.vertices, block.indices, { VOXEL_BLOCK_WIDTH, VOXEL_BLOCK_WIDTH, VOXEL_BLOCK_WIDTH });

		}, MeshReadyTag{});

		entityManager_->ForEach<VoxelBlockComponent, MeshReadyTag>
			([&](Quadbit::Entity entity, VoxelBlockComponent& block, auto& tag) {
			entityManager_->AddComponent<Quadbit::RenderMeshComponent>(entity, graphics->CreateMesh(block.vertices, sizeof(VoxelVertex), block.indices, rMeshInstance));
		});
	}

	void CulledMeshGeneration(Quadbit::Graphics* graphics, const Quadbit::QbVkRenderMeshInstance* rMeshInstance) {
		entityManager_->ParForEachAddTag<Quadbit::RenderTransformComponent, VoxelBlockComponent, MeshGenerationUpdateTag>
			([&](Quadbit::Entity entity, Quadbit::RenderTransformComponent& transform, VoxelBlockComponent& block, auto& tag) {

			VoxelMesher::CulledMeshVoxelBlock(block.voxels, block.vertices, block.indices, { VOXEL_BLOCK_WIDTH, VOXEL_BLOCK_WIDTH, VOXEL_BLOCK_WIDTH });

		}, MeshReadyTag{});

		entityManager_->ForEach<VoxelBlockComponent, MeshReadyTag>
			([&](Quadbit::Entity entity, VoxelBlockComponent& block, auto& tag) {
			entityManager_->AddComponent<Quadbit::RenderMeshComponent>(entity, graphics->CreateMesh(block.vertices, sizeof(VoxelVertex), block.indices, rMeshInstance));
		});

	}

	void Update(float dt, Quadbit::Graphics* graphics, const Quadbit::QbVkRenderMeshInstance* rMeshInstance) {
		GreedyMeshGeneration(graphics, rMeshInstance);
	}
};
