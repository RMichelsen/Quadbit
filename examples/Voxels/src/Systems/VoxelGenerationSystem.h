#pragma once

#include "Engine/Entities/EntityManager.h"
#include "Engine/Entities/ComponentSystem.h"

#include <FastNoiseSIMD/FastNoiseSIMD.h>

#include "../Utils/TerrainTools.h"
#include "../Utils/CommonUtils.h"
#include "../Data/Defines.h"
#include "../Data/Components.h"

struct VoxelGenerationSystem : Quadbit::ComponentSystem {
	void Update(float dt, FastNoiseSIMD* fastnoiseTerrain, FastNoiseSIMD* fastnoiseRegion, FastNoiseSIMD* fastnoiseColours) {
		auto& entityManager = Quadbit::EntityManager::Instance();

		entityManager.ParForEachAddTag<Quadbit::RenderTransformComponent, VoxelBlockComponent, VoxelBlockUpdateTag>
			([&](Quadbit::Entity entity, Quadbit::RenderTransformComponent& transform, VoxelBlockComponent& block, auto& tag) {
			auto& voxels = block.voxels;

			glm::int3 extents = { VOXEL_BLOCK_WIDTH, VOXEL_BLOCK_WIDTH , VOXEL_BLOCK_WIDTH };

			// This perlin set generates noise roughly in the region of -0.8f to 0.8f
			float* terrainNoise = fastnoiseTerrain->GetNoiseSet(
				static_cast<int>(transform.position.z), 0, static_cast<int>(transform.position.x), 
				VOXEL_BLOCK_WIDTH, 1, VOXEL_BLOCK_WIDTH, 0.25f
			);

			float* regionNoise = fastnoiseRegion->GetCellularSet(
				static_cast<int>(transform.position.z), 0, static_cast<int>(transform.position.x),
				VOXEL_BLOCK_WIDTH, 1, VOXEL_BLOCK_WIDTH, 1.0f
			);

			float* colourNoise = fastnoiseColours->GetCellularSet(
				static_cast<int>(transform.position.z), 0, static_cast<int>(transform.position.x),
				VOXEL_BLOCK_WIDTH, 1, VOXEL_BLOCK_WIDTH, 1.0f
			);

			for(auto z = 0; z < VOXEL_BLOCK_WIDTH; z++) {
				for(auto y = 0; y < VOXEL_BLOCK_WIDTH; y++) {
					for(auto x = 0; x < VOXEL_BLOCK_WIDTH; x++) {
						auto& voxel = voxels[CommonUtils::ConvertIndex({ x, y, z }, extents)];

						const bool belowCutoff = (y + transform.position.y) < (VOXEL_BLOCK_WIDTH * ((terrainNoise[(z * VOXEL_BLOCK_WIDTH) + x] + 0.8f) / 1.6f ));
						belowCutoff ? voxel.fillType = FillType::Solid : voxel.fillType = FillType::Empty;

						voxel.region = TerrainTools::GetRegion(regionNoise[(z * VOXEL_BLOCK_WIDTH) + x]);
						voxel.colour = TerrainTools::GetColour(voxel.region, y, { VOXEL_BLOCK_WIDTH, VOXEL_BLOCK_WIDTH, VOXEL_BLOCK_WIDTH }, colourNoise[(z * VOXEL_BLOCK_WIDTH) + x]);
					}
				}
			}

			for(auto z = 0; z < VOXEL_BLOCK_WIDTH; z++) {
				for(auto y = 0; y < VOXEL_BLOCK_WIDTH; y++) {
					for(auto x = 0; x < VOXEL_BLOCK_WIDTH; x++) {
						auto& voxel = voxels[CommonUtils::ConvertIndex({ x, y, z }, extents)];

						// Skip transparent voxels
						if(voxel.fillType == FillType::Empty) continue;

						// Set all visible faces by default
						voxel.visibleFaces = (VisibleFaces::East | VisibleFaces::West | VisibleFaces::Top | VisibleFaces::Bottom | VisibleFaces::North | VisibleFaces::South);
						
						// Unset the faces that have a solid block next to them
						if(x + 1 < VOXEL_BLOCK_WIDTH	&& voxels[CommonUtils::ConvertIndex({ x + 1, y, z }, extents)].fillType == FillType::Solid) voxel.visibleFaces &= ~VisibleFaces::East;
						if(x - 1 >= 0					&& voxels[CommonUtils::ConvertIndex({ x - 1, y, z }, extents)].fillType == FillType::Solid) voxel.visibleFaces &= ~VisibleFaces::West;
						if(y + 1 < VOXEL_BLOCK_WIDTH	&& voxels[CommonUtils::ConvertIndex({ x, y + 1, z }, extents)].fillType == FillType::Solid) voxel.visibleFaces &= ~VisibleFaces::Top;
						if(y - 1 >= 0					&& voxels[CommonUtils::ConvertIndex({ x, y - 1, z }, extents)].fillType == FillType::Solid) voxel.visibleFaces &= ~VisibleFaces::Bottom;
						if(z + 1 < VOXEL_BLOCK_WIDTH	&& voxels[CommonUtils::ConvertIndex({ x, y, z + 1 }, extents)].fillType == FillType::Solid) voxel.visibleFaces &= ~VisibleFaces::North;
						if(z - 1 >= 0					&& voxels[CommonUtils::ConvertIndex({ x, y, z - 1 }, extents)].fillType == FillType::Solid) voxel.visibleFaces &= ~VisibleFaces::South;

						// Faces that aren't part of the top are not visible
						// However if the top face is visible on this voxel, the sides are too to avoid gaps!
						if(x == VOXEL_BLOCK_WIDTH - 1 && (static_cast<uint32_t>(voxel.visibleFaces & VisibleFaces::Top) == 0)) voxel.visibleFaces &= ~VisibleFaces::East;
						if(x == 0 && (static_cast<uint32_t>(voxel.visibleFaces & VisibleFaces::Top) == 0)) voxel.visibleFaces &= ~VisibleFaces::West;
						if(y == 0 && (static_cast<uint32_t>(voxel.visibleFaces & VisibleFaces::Top) == 0)) voxel.visibleFaces &= ~VisibleFaces::Bottom;
						if(z == VOXEL_BLOCK_WIDTH - 1 && (static_cast<uint32_t>(voxel.visibleFaces & VisibleFaces::Top) == 0)) voxel.visibleFaces &= ~VisibleFaces::North;
						if(z == 0 && (static_cast<uint32_t>(voxel.visibleFaces & VisibleFaces::Top) == 0)) voxel.visibleFaces &= ~VisibleFaces::South;
					}
				}
			}
			fastnoiseTerrain->FreeNoiseSet(terrainNoise);
			fastnoiseRegion->FreeNoiseSet(regionNoise);

		}, MeshGenerationUpdateTag{});
	}
};