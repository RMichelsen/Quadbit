#pragma once

#include "../Data/Defines.h"
#include "../Data/Components.h"

#include "Engine/Rendering/QbVkRenderer.h"
#include "Engine/Entities/EntityManager.h"

struct MeshGenerationSystem : Quadbit::ComponentSystem {
	glm::int4x3 GetPointsForFaces(const VisibleFaces face) {
		static const std::array<glm::ivec3, 8> cubePoints{ {
			{0, 0, 0},
			{0, 1, 0},
			{0, 0, 1},
			{0, 1, 1},
			{1, 0, 0},
			{1, 1, 0},
			{1, 0, 1},
			{1, 1, 1}
		} };

		switch(face) {
		case VisibleFaces::North:
			return { cubePoints[6], cubePoints[7], cubePoints[3], cubePoints[2]};
		case VisibleFaces::South:
			return { cubePoints[4], cubePoints[0], cubePoints[1], cubePoints[5] };
		case VisibleFaces::East:
			return { cubePoints[4], cubePoints[5], cubePoints[7], cubePoints[6] };
		case VisibleFaces::West:
			return { cubePoints[0], cubePoints[2], cubePoints[3], cubePoints[1] };
		case VisibleFaces::Top:
			return { cubePoints[5], cubePoints[1], cubePoints[3], cubePoints[7] };
		case VisibleFaces::Bottom:
			return { cubePoints[4], cubePoints[6], cubePoints[2], cubePoints[0] };
		default:
			return { cubePoints[0], cubePoints[0], cubePoints[0], cubePoints[0] };
		}
	}

	glm::int4x3 GetPointsForFaces(const VisibleFaces face, const glm::int4x3 points) {
		glm::int3 idx3D = points[0];
		glm::int3 firstDest = points[1];
		glm::int3 secondDest = points[2];
		glm::int3 combinedDest = points[3];

		switch(face) {
		case VisibleFaces::Top:
			return { firstDest + glm::int3(0, 1, 0), idx3D + glm::int3(0, 1, 0), secondDest + glm::int3(0, 1, 0), combinedDest + glm::int3(0, 1, 0) };
		case VisibleFaces::North:
			return { firstDest + glm::int3(0, 0, 1), combinedDest + glm::int3(0, 0, 1), secondDest + glm::int3(0, 0, 1), idx3D + glm::int3(0, 0, 1) };
		case VisibleFaces::East:
			return { idx3D + glm::int3(1, 0, 0), firstDest + glm::int3(1, 0, 0), combinedDest + glm::int3(1, 0, 0), secondDest + glm::int3(1, 0, 0) };
		case VisibleFaces::Bottom:
			return { secondDest, idx3D, firstDest, combinedDest };
		case VisibleFaces::South:
			return { firstDest, idx3D, secondDest, combinedDest };
		case VisibleFaces::West:
			return { idx3D, secondDest, combinedDest, firstDest };
		default:
			return { idx3D, idx3D, idx3D, idx3D };
		}
	}

	// Returns the offsets for the ambient occlusion for a particular face.
	// The first two float3's are the sides, the last is the corner offset.
	std::array<const glm::int3x3, 4> GetAmbientOcclusionOffsets(const VisibleFaces face) {
		static const std::array<glm::int3x3, 8> aoOffsets{ {
			{{0, 0, -1}, {-1, 0, 0}, {-1, 0, -1}},
			{{0, 1, -1}, {-1, 1, 0}, {-1, 1, -1}},
			{{-1, 0, 0}, {0, 0, 1},  {-1, 0, 1}},
			{{-1, 1, 0}, {0, 1, 1},  {-1, 1, 1}},
			{{1, 0, 0},  {0, 0, -1}, {1, 0, -1}},
			{{1, 1, 0},  {0, 1, -1}, {1, 1, -1}},
			{{1, 0, 0},  {0, 0, 1},  {1, 0, 1}},
			{{1, 1, 0},  {0, 1, 1},  {1, 1, 1}}
		} };

		switch(face) {
		case VisibleFaces::North:
			return { aoOffsets[6], aoOffsets[7], aoOffsets[3], aoOffsets[2] };
		case VisibleFaces::South:	
			return { aoOffsets[4], aoOffsets[0], aoOffsets[1], aoOffsets[5] };
		case VisibleFaces::East:	
			return { aoOffsets[4], aoOffsets[5], aoOffsets[7], aoOffsets[6] };
		case VisibleFaces::West:	
			return { aoOffsets[0], aoOffsets[2], aoOffsets[3], aoOffsets[1] };
		case VisibleFaces::Top:		
			return { aoOffsets[5], aoOffsets[1], aoOffsets[3], aoOffsets[7] };
		case VisibleFaces::Bottom:	
			return { aoOffsets[4], aoOffsets[6], aoOffsets[2], aoOffsets[0] };
		default:					
			return { aoOffsets[0], aoOffsets[0], aoOffsets[0], aoOffsets[0] };
		}
	}


	void AddQuad(const VisibleFaces face, const glm::ivec3 position, const glm::vec3 colour, glm::float4 ambientOcclusion,
		std::vector<VoxelVertex>& vertices, std::vector<uint32_t>& indices) {
		glm::int4x3 points = GetPointsForFaces(face);
		for(int i = 0; i < 4; i++) {
			vertices.push_back({ points[i] + position, colour * ambientOcclusion[i] });
		}
		auto vertexCount = static_cast<uint32_t>(vertices.size());

		const bool flip = (ambientOcclusion.x + ambientOcclusion.z < ambientOcclusion.y + ambientOcclusion.w);
		indices.push_back(vertexCount - 4 + 0);
		indices.push_back(vertexCount - 4 + 1);
		flip ? indices.push_back(vertexCount - 4 + 3) : indices.push_back(vertexCount - 4 + 2);

		flip ? indices.push_back(vertexCount - 4 + 1) : indices.push_back(vertexCount - 4 + 0);
		indices.push_back(vertexCount - 4 + 2);
		indices.push_back(vertexCount - 4 + 3);
	}

	void AddQuad(const VisibleFaces face, const glm::int4x3 points, const glm::vec3 colour, glm::float4 ambientOcclusion,
		std::vector<VoxelVertex>& vertices, std::vector<uint32_t>& indices) {
		glm::int4x3 translatedPoints = GetPointsForFaces(face, points);
		for(int i = 0; i < 4; i++) {
			vertices.push_back({ translatedPoints[i], colour * ambientOcclusion[i] });
		}
		auto vertexCount = static_cast<uint32_t>(vertices.size());

		const bool flip = (ambientOcclusion.x + ambientOcclusion.z < ambientOcclusion.y + ambientOcclusion.w);
		indices.push_back(vertexCount - 4 + 0);
		indices.push_back(vertexCount - 4 + 1);
		flip ? indices.push_back(vertexCount - 4 + 3) : indices.push_back(vertexCount - 4 + 2);

		flip ? indices.push_back(vertexCount - 4 + 1) : indices.push_back(vertexCount - 4 + 0);
		indices.push_back(vertexCount - 4 + 2);
		indices.push_back(vertexCount - 4 + 3);
	}

	// Converts (flattens) a 3D index
	uint32_t ConvertIndex(glm::int3 index) {
		return index.x + VOXEL_BLOCK_WIDTH * (index.y + VOXEL_BLOCK_WIDTH * index.z);
	}

	// Important: Side 3 is the corner side
	float CalculateVertexOcclusion(glm::int3x3 sides, const std::vector<Voxel>& voxels) {
		static const glm::float4 OCCLUSION_CURVE = glm::float4{ 0.55f, 0.7f, 0.85f, 1.0f };

		const bool ocSide1 = (sides[0].x < VOXEL_BLOCK_WIDTH && sides[0].x >= 0 && sides[0].y < VOXEL_BLOCK_WIDTH && sides[0].y >= 0 && sides[0].z < VOXEL_BLOCK_WIDTH && sides[0].z >= 0)
			? (voxels[ConvertIndex(sides[0])].fillType == Solid) : false;
		const bool ocSide2 = (sides[1].x < VOXEL_BLOCK_WIDTH && sides[1].x >= 0 && sides[1].y < VOXEL_BLOCK_WIDTH && sides[1].y >= 0 && sides[1].z < VOXEL_BLOCK_WIDTH && sides[1].z >= 0)
			? (voxels[ConvertIndex(sides[1])].fillType == Solid) : false;
		const bool ocCorner = (sides[2].x < VOXEL_BLOCK_WIDTH && sides[2].x >= 0 && sides[2].y < VOXEL_BLOCK_WIDTH && sides[2].y >= 0 && sides[2].z < VOXEL_BLOCK_WIDTH && sides[2].z >= 0)
			? (voxels[ConvertIndex(sides[2])].fillType == Solid) : false;

		if(ocSide1 && ocSide2) {
			return OCCLUSION_CURVE[0];
		}

		return OCCLUSION_CURVE[3 - (ocSide1 + ocSide2 + ocCorner)];
	}

	glm::float4 CalculateFaceAmbientOcclusion(const VisibleFaces face, const glm::ivec3 position, const std::vector<Voxel>& voxels) {
		auto ambientOcclusionOffsets = GetAmbientOcclusionOffsets(face);

		return {
			CalculateVertexOcclusion({ambientOcclusionOffsets[0][0] + position, ambientOcclusionOffsets[0][1] + position, ambientOcclusionOffsets[0][2] + position}, voxels),
			CalculateVertexOcclusion({ambientOcclusionOffsets[1][0] + position, ambientOcclusionOffsets[1][1] + position, ambientOcclusionOffsets[1][2] + position}, voxels),
			CalculateVertexOcclusion({ambientOcclusionOffsets[2][0] + position, ambientOcclusionOffsets[2][1] + position, ambientOcclusionOffsets[2][2] + position}, voxels),
			CalculateVertexOcclusion({ambientOcclusionOffsets[3][0] + position, ambientOcclusionOffsets[3][1] + position, ambientOcclusionOffsets[3][2] + position}, voxels)
		};
	}

	glm::int4x3 GetVertexPositions(const VisibleFaces face, glm::int4x3 offsets) {
		switch(face) {
		case VisibleFaces::North:
			return { offsets[0] + glm::int3(0, 0, 1), offsets[1] + glm::int3(0, 0, 1), offsets[2] + glm::int3(0, 0, 1), offsets[3] + glm::int3(0, 0, 1) };
		case VisibleFaces::East:
			return { offsets[0] + glm::int3(1, 0, 0), offsets[1] + glm::int3(1, 0, 0), offsets[2] + glm::int3(1, 0, 0), offsets[3] + glm::int3(1, 0, 0) };
		case VisibleFaces::Bottom:
			return offsets;
		case VisibleFaces::West:
			return { offsets[0], offsets[3], offsets[2], offsets[1] };
		case VisibleFaces::Top:
			return { offsets[0] + glm::int3(0, 1, 0), offsets[3] + glm::int3(0, 1, 0), offsets[2] + glm::int3(0, 1, 0), offsets[1] + glm::int3(0, 1, 0) };
		case VisibleFaces::South:
			return { offsets[0], offsets[3], offsets[2], offsets[1] };
		default:
			return offsets;
		}
	}

	// For a given face, returns a direction for the greedy meshing
	glm::int2x3 GetDirectionVectors(const VisibleFaces face) {
		switch(face) {
		case VisibleFaces::North:
		case VisibleFaces::South:
			return { {1, 0, 0}, {0, 1, 0} };
		case VisibleFaces::East:
		case VisibleFaces::West:
			return { {0, 1, 0}, {0, 0, 1} };
		case VisibleFaces::Top:
		case VisibleFaces::Bottom:
			return { {1, 0, 0}, {0, 0, 1} };
		default:
			return { {0, 0, 0}, {0, 0, 0} };
		}
	}

	glm::int2x3 GetAxisEndpoints(glm::int2x3 dirs, glm::int3 voxelPosition) {
		glm::int2x3 axisVectors{ voxelPosition, voxelPosition };

		if(dirs[0].x == 1) axisVectors[0].x = VOXEL_BLOCK_WIDTH;
		else if(dirs[0].y == 1) axisVectors[0].y = VOXEL_BLOCK_WIDTH;
		else if(dirs[0].z == 1) axisVectors[0].z = VOXEL_BLOCK_WIDTH;
		if(dirs[1].x == 1) axisVectors[1].x = VOXEL_BLOCK_WIDTH;
		else if(dirs[1].y == 1) axisVectors[1].y = VOXEL_BLOCK_WIDTH;
		else if(dirs[1].z == 1) axisVectors[1].z = VOXEL_BLOCK_WIDTH;

		return axisVectors;
	}

	void GreedyMeshGeneration(Quadbit::QbVkRenderer* renderer, std::shared_ptr<Quadbit::QbVkRenderMeshInstance> rMeshInstance) {
		auto entityManager = Quadbit::EntityManager::GetOrCreate();

		entityManager->ParForEach<Quadbit::RenderTransformComponent, VoxelBlockComponent, MeshGenerationUpdateTag>
			([&](Quadbit::Entity entity, Quadbit::RenderTransformComponent& transform, VoxelBlockComponent& block, auto& tag) {

			block.vertices.clear();
			block.indices.clear();
			std::vector<bool> visited;
			visited.resize(VOXEL_BLOCK_SIZE);

			for(uint32_t face = static_cast<uint32_t>(VisibleFaces::North); face <= static_cast<uint32_t>(VisibleFaces::Bottom); face *= 2) {
				visited.assign(VOXEL_BLOCK_SIZE, false);
				glm::int2x3 dirVectors = GetDirectionVectors(static_cast<VisibleFaces>(face));

				for(auto z = 0; z < VOXEL_BLOCK_WIDTH; z++) {
					for(auto y = 0; y < VOXEL_BLOCK_WIDTH; y++) {
						for(auto x = 0; x < VOXEL_BLOCK_WIDTH; x++) {
							glm::int3 idx3D = { x, y, z };
							uint32_t idx1D = ConvertIndex(idx3D);

							// Skip if face isn't visible
							if(static_cast<uint32_t>(block.voxels[idx1D].visibleFaces & static_cast<VisibleFaces>(face)) == 0) continue;
							// Skip if filltype is empty
							if(block.voxels[idx1D].fillType == FillType::Empty) continue;
							// Skip if voxel has been visited
							if(visited[idx1D]) continue;

							// Calculate ambient occlusion
							glm::float4 ao = CalculateFaceAmbientOcclusion(static_cast<VisibleFaces>(face), idx3D, block.voxels);

							// Get axis endpoints
							glm::int2x3 axesEnds = GetAxisEndpoints(dirVectors, idx3D);

							// Run along the first axis until the endpoint is reached
							glm::int3 firstDest;
							for(firstDest = idx3D; firstDest != axesEnds[0]; firstDest += dirVectors[0]) {
								auto idx = ConvertIndex(firstDest);

								// Make sure the voxel is visible and filled, and not visited previously, and that the AO values are the same as the current voxel
								if(static_cast<uint32_t>(block.voxels[idx].visibleFaces & static_cast<VisibleFaces>(face)) == 0 ||
									block.voxels[idx].fillType == FillType::Empty || visited[idx] ||
									(ao != CalculateFaceAmbientOcclusion(static_cast<VisibleFaces>(face), firstDest, block.voxels))) {
									break;
								}
								visited[idx] = true;
							}

							// Run along the second axis
							glm::int3 secondDest;
							glm::int3 combinedDest = firstDest + dirVectors[1];
							for(secondDest = idx3D + dirVectors[1]; secondDest != axesEnds[1]; secondDest += dirVectors[1], combinedDest += dirVectors[1]) {
								glm::int3 firstDirSubRun;

								for(firstDirSubRun = secondDest; firstDirSubRun != combinedDest; firstDirSubRun += dirVectors[0]) {
									auto idx = ConvertIndex(firstDirSubRun);

									// Make sure the voxel is visible and filled, and not visited previously, and that the AO values are the same as the current voxel
									if(static_cast<uint32_t>(block.voxels[idx].visibleFaces & static_cast<VisibleFaces>(face)) == 0 ||
										block.voxels[idx].fillType == FillType::Empty || visited[idx] ||
										(ao != CalculateFaceAmbientOcclusion(static_cast<VisibleFaces>(face), firstDirSubRun, block.voxels))) {
										break;
									}
								}

								// If the sub run didn't end we don't have to add any visited voxels
								// Otherwise we have to add them
								if(firstDirSubRun != combinedDest) break;
								else {
									for(firstDirSubRun = secondDest; firstDirSubRun != combinedDest; firstDirSubRun += dirVectors[0]) {
										visited[ConvertIndex(firstDirSubRun)] = true;
									}
								}
							}

							const float heightCoeff = static_cast<float>(y) / VOXEL_BLOCK_WIDTH;
							glm::vec3 colour = glm::vec3(0.4f + (heightCoeff * 0.4f), 0.7f - (heightCoeff * 0.2f), 0.1f);
							AddQuad(static_cast<VisibleFaces>(face), { idx3D, firstDest, secondDest, combinedDest }, colour, ao, block.vertices, block.indices);
						}
					}
				}
			}
		}, MeshReadyTag{});

		entityManager->ForEach<VoxelBlockComponent, MeshReadyTag>
			([&](Quadbit::Entity entity, VoxelBlockComponent& block, auto& tag) {
			entity.AddComponent<Quadbit::RenderMeshComponent>(renderer->CreateMesh(block.vertices, sizeof(VoxelVertex), block.indices, rMeshInstance));
		});
	}

	void CulledMeshGeneration(Quadbit::QbVkRenderer* renderer, std::shared_ptr<Quadbit::QbVkRenderMeshInstance> rMeshInstance) {
		auto entityManager = Quadbit::EntityManager::GetOrCreate();

		entityManager->ParForEach<Quadbit::RenderTransformComponent, VoxelBlockComponent, MeshGenerationUpdateTag>
			([&](Quadbit::Entity entity, Quadbit::RenderTransformComponent& transform, VoxelBlockComponent& block, auto& tag) {

			block.vertices.clear();
			block.indices.clear();

			std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();

			for(uint32_t face = static_cast<uint32_t>(VisibleFaces::North); face <= static_cast<uint32_t>(VisibleFaces::Bottom); face *= 2) {
				for(auto z = 0; z < VOXEL_BLOCK_WIDTH; z++) {
					for(auto y = 0; y < VOXEL_BLOCK_WIDTH; y++) {
						for(auto x = 0; x < VOXEL_BLOCK_WIDTH; x++) {
							glm::int3 idx3D = { x,y,z };
							uint32_t idx1D = ConvertIndex(idx3D);

							// Skip if face isn't visible
							if(static_cast<uint32_t>(block.voxels[idx1D].visibleFaces & static_cast<VisibleFaces>(face)) == 0) continue;
							// Skip if filltype is empty
							if(block.voxels[idx1D].fillType == FillType::Empty) continue;

							const float heightCoeff = static_cast<float>(y) / VOXEL_BLOCK_WIDTH;
							glm::vec3 colour = glm::vec3(0.4f + (heightCoeff * 0.4f), 0.7f - (heightCoeff * 0.2f), 0.1f);

							glm::float4 ao = CalculateFaceAmbientOcclusion(static_cast<VisibleFaces>(face), idx3D, block.voxels);
							AddQuad(static_cast<VisibleFaces>(face), idx3D, colour, ao, block.vertices, block.indices);
						}
					}
				}
			}
		}, MeshReadyTag{});

		entityManager->ForEach<VoxelBlockComponent, MeshReadyTag>
			([&](Quadbit::Entity entity, VoxelBlockComponent& block, auto& tag) {
			entity.AddComponent<Quadbit::RenderMeshComponent>(renderer->CreateMesh(block.vertices, sizeof(VoxelVertex), block.indices, rMeshInstance));
		});

	}

	void Update(float dt, Quadbit::QbVkRenderer* renderer, std::shared_ptr<Quadbit::QbVkRenderMeshInstance> rMeshInstance) {
		GreedyMeshGeneration(renderer, rMeshInstance);
	}
};
