#pragma once

#include "../Data/Defines.h"
#include "../Data/Components.h"
#include "../Utils/TerrainTools.h"
#include "../Utils/VoxelMesher.h"

#include "Engine/API/Graphics.h"
#include "Engine/Entities/EntityManager.h"


struct MeshGenerationSystem : Quadbit::ComponentSystem {
	void GreedyMeshGeneration(Quadbit::Graphics* const graphics, const Quadbit::QbVkPipelineHandle pipeline) {
		entityManager_->ParForEachAddTag<Quadbit::RenderTransformComponent, VoxelBlockComponent, MeshGenerationUpdateTag>
			([&](Quadbit::Entity entity, Quadbit::RenderTransformComponent& transform, VoxelBlockComponent& block, auto& tag) {

			VoxelMesher::GreedyMeshVoxelBlock(block.voxels, block.vertices, block.indices, { VOXEL_BLOCK_WIDTH, VOXEL_BLOCK_WIDTH, VOXEL_BLOCK_WIDTH });

		}, MeshReadyTag{});

		entityManager_->ForEach<VoxelBlockComponent, MeshReadyTag>
			([&](Quadbit::Entity entity, VoxelBlockComponent& block, auto& tag) {
			entityManager_->AddComponent<Quadbit::CustomMeshComponent>(entity, graphics->CreateMesh(block.vertices, sizeof(VoxelVertex), block.indices, pipeline));
		});
	}

	void CulledMeshGeneration(Quadbit::Graphics* const graphics, const Quadbit::QbVkPipelineHandle pipeline) {
		entityManager_->ParForEachAddTag<Quadbit::RenderTransformComponent, VoxelBlockComponent, MeshGenerationUpdateTag>
			([&](Quadbit::Entity entity, Quadbit::RenderTransformComponent& transform, VoxelBlockComponent& block, auto& tag) {

			VoxelMesher::CulledMeshVoxelBlock(block.voxels, block.vertices, block.indices, { VOXEL_BLOCK_WIDTH, VOXEL_BLOCK_WIDTH, VOXEL_BLOCK_WIDTH });

		}, MeshReadyTag{});

		entityManager_->ForEach<VoxelBlockComponent, MeshReadyTag>
			([&](Quadbit::Entity entity, VoxelBlockComponent& block, auto& tag) {
			entityManager_->AddComponent<Quadbit::CustomMeshComponent>(entity, graphics->CreateMesh(block.vertices, sizeof(VoxelVertex), block.indices, pipeline));
		});

	}

	void Update(float dt, Quadbit::Graphics* const graphics, const Quadbit::QbVkPipelineHandle pipeline) {
		GreedyMeshGeneration(graphics, pipeline);
	}
};
