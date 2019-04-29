#pragma once

#include "../Data/Defines.h"
#include "../Data/Components.h"
#include "../Engine/Rendering/QbVkRenderer.h"
#include "../Engine/Entities/EntityManager.h"
#include "../Engine/Entities/ComponentSystem.h"
#include "../Engine/Rendering/RenderTypes.h"

struct MeshGenerationSystem : Quadbit::ComponentSystem {
	// Converts (flattens) a 3D index
	uint32_t ConvertIndex(uint32_t x, uint32_t y, uint32_t z) {
		return x + VOXEL_BLOCK_WIDTH * (y + VOXEL_BLOCK_WIDTH * z);
	}

	void AddQuad(const VisibleFaces face, std::vector<Quadbit::MeshVertex>& vertices, std::vector<uint32_t>& indices, glm::float4x3 offsets, glm::vec3 colour) {
		switch(face) {
		case VisibleFaces::North:
			vertices.push_back({ glm::vec3(1, 0, 1) + offsets[0], colour });
			vertices.push_back({ glm::vec3(1, 1, 1) + offsets[1], colour });
			vertices.push_back({ glm::vec3(0, 1, 1) + offsets[2], colour });
			vertices.push_back({ glm::vec3(0, 0, 1) + offsets[3], colour });
			break;
		case VisibleFaces::South:
			vertices.push_back({ glm::vec3(1, 0, 0) + offsets[0], colour });
			vertices.push_back({ glm::vec3(0, 0, 0) + offsets[1], colour });
			vertices.push_back({ glm::vec3(0, 1, 0) + offsets[2], colour });
			vertices.push_back({ glm::vec3(1, 1, 0) + offsets[3], colour });
			break;
		case VisibleFaces::East:
			vertices.push_back({ glm::vec3(1, 0, 0) + offsets[0], colour});
			vertices.push_back({ glm::vec3(1, 1, 0) + offsets[1], colour});
			vertices.push_back({ glm::vec3(1, 1, 1) + offsets[2], colour});
			vertices.push_back({ glm::vec3(1, 0, 1) + offsets[3], colour});
			break;
		case VisibleFaces::West:
			vertices.push_back({ glm::vec3(0, 0, 0) + offsets[0], colour});
			vertices.push_back({ glm::vec3(0, 0, 1) + offsets[1], colour});
			vertices.push_back({ glm::vec3(0, 1, 1) + offsets[2], colour});
			vertices.push_back({ glm::vec3(0, 1, 0) + offsets[3], colour});
			break;
		case VisibleFaces::Top:
			vertices.push_back({ glm::vec3(1, 1, 0) + offsets[0], colour});
			vertices.push_back({ glm::vec3(0, 1, 0) + offsets[1], colour});
			vertices.push_back({ glm::vec3(0, 1, 1) + offsets[2], colour});
			vertices.push_back({ glm::vec3(1, 1, 1) + offsets[3], colour});
			break;
		case VisibleFaces::Bottom:
			vertices.push_back({ glm::vec3(1, 0, 0) + offsets[0], colour});
			vertices.push_back({ glm::vec3(1, 0, 1) + offsets[1], colour});
			vertices.push_back({ glm::vec3(0, 0, 1) + offsets[2], colour});
			vertices.push_back({ glm::vec3(0, 0, 0) + offsets[3], colour});
			break;
		}
		auto vertexCount = static_cast<uint32_t>(vertices.size());

		indices.push_back(vertexCount - 4 + 0);
		indices.push_back(vertexCount - 4 + 1);
		indices.push_back(vertexCount - 4 + 2);

		indices.push_back(vertexCount - 4 + 0);
		indices.push_back(vertexCount - 4 + 2);
		indices.push_back(vertexCount - 4 + 3);
	}

	void Update(float dt, Quadbit::QbVkRenderer* renderer) {
		auto entityManager = Quadbit::EntityManager::GetOrCreate();

		entityManager->ForEach<VoxelBlockComponent, MeshGenerationUpdateTag>([&](Quadbit::Entity entity, VoxelBlockComponent& block, auto& tag) {
			std::vector<Quadbit::MeshVertex> vertices;
			std::vector<uint32_t> indices;
			vertices.reserve(VOXEL_BLOCK_SIZE);
			indices.reserve(VOXEL_BLOCK_SIZE);

			auto& voxels = block.voxels;

			for(uint32_t face = static_cast<uint32_t>(VisibleFaces::North); face <= static_cast<uint32_t>(VisibleFaces::Bottom); face *= 2) {
				for(auto z = 0; z < VOXEL_BLOCK_WIDTH; z++) {
					for(auto y = 0; y < VOXEL_BLOCK_WIDTH; y++) {
						for(auto x = 0; x < VOXEL_BLOCK_WIDTH; x++) {
							auto& voxel = voxels[ConvertIndex(x, y, z)];

							// Skip if face isn't visible
							if(static_cast<uint32_t>(voxel.visibleFaces & static_cast<VisibleFaces>(face)) == 0) continue;
							// Skip if filltype is empty
							if(voxel.fillType == FillType::Empty) continue;

							glm::vec3 offset = glm::vec3(x, y, z);

							const float heightCoeff = static_cast<float>(y) / VOXEL_BLOCK_WIDTH;
							glm::vec3 colour = glm::vec3(0.2f + (0.22f * heightCoeff), 0.4f - (0.33f * heightCoeff), 0.2f - (0.11f * heightCoeff));

							AddQuad(static_cast<VisibleFaces>(face), vertices, indices, { offset, offset, offset, offset }, colour);
						}
					}
				}
			}

			entity.AddComponent<Quadbit::RenderMeshComponent>(renderer->CreateMesh(vertices, indices));
		});
	}
};
