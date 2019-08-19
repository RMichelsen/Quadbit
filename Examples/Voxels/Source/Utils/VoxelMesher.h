#pragma once

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include "../Data/Components.h"
#include "CommonUtils.h"

namespace VoxelMesher {
	struct VoxelMesh {
		std::vector<VoxelVertex> vertices;
		std::vector<uint32_t> indices;
	};

	// For a given face, returns a direction for the greedy meshing
	inline glm::int2x3 GetDirectionVectors(const VisibleFaces face) {
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

	// Returns the offsets for the ambient occlusion for a particular face.
	// The first two float3's are the sides, the last is the corner offset.
	inline std::array<const glm::int3x3, 4> GetAmbientOcclusionOffsets(const VisibleFaces face) {
		switch(face) {
		case VisibleFaces::Top:
			return {
				glm::int3x3 {{-1, 1, 0},  {0, 1, -1}, {-1, 1, -1}}, glm::int3x3 {{-1, 1, 0}, {0, 1, 1}, {-1, 1, 1}},
				glm::int3x3 {{1, 1, 0},   {0, 1, 1},  {1, 1, 1}},   glm::int3x3  {{1, 1, 0}, {0, 1, -1}, {1, 1, -1}}
			};
		case VisibleFaces::South:
			return {
				glm::int3x3 {{0, -1, -1},  {-1, 0, -1}, {-1, -1,-1}}, glm::int3x3 {{0, 1, -1}, {-1, 0, -1}, {-1, 1, -1}},
				glm::int3x3 {{0, 1, -1},   {1, 0, -1},  {1, 1, -1}},  glm::int3x3 {{0, -1, -1}, {1, 0, -1}, {1, -1, -1}}
			};
		case VisibleFaces::West:
			return {
				glm::int3x3 {{-1, -1, 0}, {-1, 0, -1}, {-1, -1, -1}}, glm::int3x3 {{-1, -1, 0},  {-1, 0, 1}, {-1, -1, 1}},
				glm::int3x3 {{-1, 1, 0}, {-1, 0, 1}, {-1, 1, 1}}, glm::int3x3 {{-1, 1, 0},   {-1, 0, -1},  {-1, 1, -1}}
			};
		case VisibleFaces::Bottom:
			return {
				glm::int3x3 {{-1, -1, 0},  {0, -1, -1}, {-1, -1, -1}}, glm::int3x3  {{1, -1, 0}, {0, -1, -1}, {1, -1, -1}},
				glm::int3x3 {{1, -1, 0},   {0, -1, 1},  {1, -1, 1}}, glm::int3x3 {{-1, -1, 0}, {0, -1, 1}, {-1, -1, 1}}
			};
		case VisibleFaces::North:
			return {
				glm::int3x3 {{0, -1, 1},  {-1, 0, 1}, {-1, -1,1}}, glm::int3x3 {{0, -1, 1}, {1, 0, 1}, {1, -1, 1}},
				glm::int3x3 {{0, 1, 1},   {1, 0, 1},  {1, 1, 1}}, glm::int3x3 {{0, 1, 1}, {-1, 0, 1}, {-1, 1, 1}}
			};
		case VisibleFaces::East:
			return {
				glm::int3x3 {{1, -1, 0}, {1, 0, -1}, {1, -1, -1}}, glm::int3x3 {{1, 1, 0},   {1, 0, -1},  {1, 1, -1}},
				glm::int3x3 {{1, 1, 0}, {1, 0, 1}, {1, 1, 1}}, glm::int3x3 {{1, -1, 0},  {1, 0, 1}, {1, -1, 1}}
			};
		default:
			return { glm::int3x3 {}, glm::int3x3 {}, glm::int3x3 {} };
		}
	}

	// Important: Side 3 is the corner side
	inline float CalculateVertexOcclusion(const glm::int3x3 sides, const std::vector<Voxel>& voxels, const glm::int3 extents) {
		static const glm::float4 OCCLUSION_CURVE = glm::float4{ 0.5f, 0.7f, 0.85f, 1.0f };

		const bool ocSide1 = (sides[0].x < extents.x && sides[0].x >= 0 && sides[0].y < extents.y && sides[0].y >= 0 && sides[0].z < extents.z && sides[0].z >= 0)
			? (voxels[CommonUtils::ConvertIndex(sides[0], extents)].fillType == FillType::Solid) : false;
		const bool ocSide2 = (sides[1].x < extents.x && sides[1].x >= 0 && sides[1].y < extents.y && sides[1].y >= 0 && sides[1].z < extents.z && sides[1].z >= 0)
			? (voxels[CommonUtils::ConvertIndex(sides[1], extents)].fillType == FillType::Solid) : false;
		const bool ocCorner = (sides[2].x < extents.x && sides[2].x >= 0 && sides[2].y < extents.y && sides[2].y >= 0 && sides[2].z < extents.z && sides[2].z >= 0)
			? (voxels[CommonUtils::ConvertIndex(sides[2], extents)].fillType == FillType::Solid) : false;

		if(ocSide1 && ocSide2) {
			return OCCLUSION_CURVE[0];
		}

		return OCCLUSION_CURVE[3 - (ocSide1 + ocSide2 + ocCorner)];
	}

	inline glm::float4 CalculateFaceAmbientOcclusion(const VisibleFaces face, const glm::int3 position, const std::vector<Voxel>& voxels, const glm::int3 extents) {
		auto ambientOcclusionOffsets = GetAmbientOcclusionOffsets(face);

		return {
			CalculateVertexOcclusion({ambientOcclusionOffsets[0][0] + position, ambientOcclusionOffsets[0][1] + position, ambientOcclusionOffsets[0][2] + position}, voxels, extents),
			CalculateVertexOcclusion({ambientOcclusionOffsets[1][0] + position, ambientOcclusionOffsets[1][1] + position, ambientOcclusionOffsets[1][2] + position}, voxels, extents),
			CalculateVertexOcclusion({ambientOcclusionOffsets[2][0] + position, ambientOcclusionOffsets[2][1] + position, ambientOcclusionOffsets[2][2] + position}, voxels, extents),
			CalculateVertexOcclusion({ambientOcclusionOffsets[3][0] + position, ambientOcclusionOffsets[3][1] + position, ambientOcclusionOffsets[3][2] + position}, voxels, extents)
		};
	}

	inline glm::int2x3 GetAxisEndpoints(const glm::int2x3 dirs, const glm::int3 voxelPosition, const glm::int3 extents) {
		glm::int2x3 axisVectors{ voxelPosition, voxelPosition };

		if(dirs[0].x == 1) axisVectors[0].x = extents.x;
		else if(dirs[0].y == 1) axisVectors[0].y = extents.y;
		else if(dirs[0].z == 1) axisVectors[0].z = extents.z;
		if(dirs[1].x == 1) axisVectors[1].x = extents.x;
		else if(dirs[1].y == 1) axisVectors[1].y = extents.y;
		else if(dirs[1].z == 1) axisVectors[1].z = extents.z;

		return axisVectors;
	}

	inline glm::int4x3 GetPointsForFaces(const VisibleFaces face, const glm::int4x3 points) {
		glm::int3 idx3D = points[0];
		glm::int3 firstDest = points[1];
		glm::int3 secondDest = points[2];
		glm::int3 combinedDest = points[3];

		switch(face) {
		case VisibleFaces::Top:
			return { idx3D + glm::int3(0, 1, 0), secondDest + glm::int3(0, 1, 0), combinedDest + glm::int3(0, 1, 0), firstDest + glm::int3(0, 1, 0) };
		case VisibleFaces::South:
			return { idx3D, secondDest, combinedDest, firstDest };
		case VisibleFaces::West:
			return { idx3D, secondDest, combinedDest, firstDest };
		case VisibleFaces::Bottom:
			return { idx3D, firstDest, combinedDest, secondDest };
		case VisibleFaces::North:
			return { idx3D + glm::int3(0, 0, 1), firstDest + glm::int3(0, 0, 1), combinedDest + glm::int3(0, 0, 1), secondDest + glm::int3(0, 0, 1) };
		case VisibleFaces::East:
			return { idx3D + glm::int3(1, 0, 0), firstDest + glm::int3(1, 0, 0), combinedDest + glm::int3(1, 0, 0), secondDest + glm::int3(1, 0, 0) };

		default:
			return { idx3D, idx3D, idx3D, idx3D };
		}
	}

	inline glm::int4x3 GetPointsForFaces(const VisibleFaces face) {
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
			return { cubePoints[6], cubePoints[7], cubePoints[3], cubePoints[2] };
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

	inline void AddQuad(const VisibleFaces face, const glm::int4x3 points, const glm::vec3 colour, glm::float4 ambientOcclusion,
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

	inline void AddQuad(const VisibleFaces face, const glm::int3 position, const glm::vec3 colour, glm::float4 ambientOcclusion,
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

	inline void GreedyMeshVoxelBlock(const std::vector<Voxel>& voxels, std::vector<VoxelVertex>& vertices, std::vector<uint32_t>& indices, glm::int3 extents) {
		vertices.clear();
		indices.clear();

		uint64_t voxelBlockSize = static_cast<uint64_t>(extents.x) * static_cast<uint64_t>(extents.y) * static_cast<uint64_t>(extents.z);

		std::vector<bool> visited;
		visited.resize(voxelBlockSize);

		for(uint32_t face = static_cast<uint32_t>(VisibleFaces::North); face <= static_cast<uint32_t>(VisibleFaces::Bottom); face *= 2) {
			visited.assign(voxelBlockSize, false);
			glm::int2x3 dirVectors = GetDirectionVectors(static_cast<VisibleFaces>(face));

			for(auto z = 0; z < extents.z; z++) {
				for(auto y = 0; y < extents.y; y++) {
					for(auto x = 0; x < extents.x; x++) {
						glm::int3 idx3D = { x, y, z };
						uint32_t idx1D = CommonUtils::ConvertIndex(idx3D, extents);

						// Skip if face isn't visible
						if(static_cast<uint32_t>(voxels[idx1D].visibleFaces & static_cast<VisibleFaces>(face)) == 0) continue;
						// Skip if filltype is empty
						if(voxels[idx1D].fillType == FillType::Empty) continue;
						// Skip if voxel has been visited
						if(visited[idx1D]) continue;

						// Calculate ambient occlusion
						glm::float4 ao = CalculateFaceAmbientOcclusion(static_cast<VisibleFaces>(face), idx3D, voxels, extents);

						// Get axis endpoints
						glm::int2x3 axesEnds = GetAxisEndpoints(dirVectors, idx3D, extents);

						// Run along the first axis until the endpoint is reached
						glm::int3 firstDest;
						for(firstDest = idx3D; firstDest != axesEnds[0]; firstDest += dirVectors[0]) {
							auto idx = CommonUtils::ConvertIndex(firstDest, extents);

							// Make sure the voxel is visible and filled, and not visited previously, and that the AO values are the same as the current voxel
							if(static_cast<uint32_t>(voxels[idx].visibleFaces & static_cast<VisibleFaces>(face)) == 0 ||
								voxels[idx].fillType == FillType::Empty || visited[idx] ||
								ao != CalculateFaceAmbientOcclusion(static_cast<VisibleFaces>(face), firstDest, voxels, extents) ||
								voxels[idx1D].colour != voxels[idx].colour) {
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
								auto idx = CommonUtils::ConvertIndex(firstDirSubRun, extents);

								// Make sure the voxel is visible and filled, and not visited previously, and that the AO values are the same as the current voxel
								if(static_cast<uint32_t>(voxels[idx].visibleFaces & static_cast<VisibleFaces>(face)) == 0 ||
									voxels[idx].fillType == FillType::Empty || visited[idx] ||
									ao != CalculateFaceAmbientOcclusion(static_cast<VisibleFaces>(face), firstDirSubRun, voxels, extents) ||
									voxels[idx1D].colour != voxels[idx].colour) {
									break;
								}
							}

							// If the sub run didn't end we don't have to add any visited voxels
							// Otherwise we have to add them
							if(firstDirSubRun != combinedDest) break;
							else {
								for(firstDirSubRun = secondDest; firstDirSubRun != combinedDest; firstDirSubRun += dirVectors[0]) {
									visited[CommonUtils::ConvertIndex(firstDirSubRun, extents)] = true;
								}
							}
						}

						//glm::vec3 colour = glm::vec3(0.4f + (heightCoeff * 0.4f), 0.7f - (heightCoeff * 0.2f), 0.1f);
						AddQuad(static_cast<VisibleFaces>(face), { idx3D, firstDest, secondDest, combinedDest }, voxels[idx1D].colour, ao, vertices, indices);
					}
				}
			}
		}
	}

	inline void CulledMeshVoxelBlock(const std::vector<Voxel>& voxels, std::vector<VoxelVertex>& vertices, std::vector<uint32_t>& indices, glm::int3 extents) {
		vertices.clear();
		indices.clear();

		for(uint32_t face = static_cast<uint32_t>(VisibleFaces::North); face <= static_cast<uint32_t>(VisibleFaces::Bottom); face *= 2) {
			for(auto z = 0; z < extents.z; z++) {
				for(auto y = 0; y < extents.y; y++) {
					for(auto x = 0; x < extents.x; x++) {
						glm::int3 idx3D = { x, y, z };
						uint32_t idx1D = CommonUtils::ConvertIndex(idx3D, extents);

						// Skip if face isn't visible
						if(static_cast<uint32_t>(voxels[idx1D].visibleFaces & static_cast<VisibleFaces>(face)) == 0) continue;
						// Skip if filltype is empty
						if(voxels[idx1D].fillType == FillType::Empty) continue;

						glm::float4 ao = CalculateFaceAmbientOcclusion(static_cast<VisibleFaces>(face), idx3D, voxels, extents);
						AddQuad(static_cast<VisibleFaces>(face), idx3D, voxels[idx1D].colour, ao, vertices, indices);
					}
				}
			}
		}
	}
}