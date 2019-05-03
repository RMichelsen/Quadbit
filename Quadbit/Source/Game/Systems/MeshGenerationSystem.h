#pragma once

#include "../Data/Defines.h"
#include "../Data/Components.h"
#include "../Engine/Rendering/QbVkRenderer.h"
#include "../Engine/Rendering/RenderTypes.h"
#include "../Engine/Entities/EntityManager.h"
#include "../Engine/Entities/ComponentSystem.h"
#include "../Engine/Physics/PhysicsComponents.h"
#include "../Engine/Physics/PxCompat.h"

struct MeshGenerationSystem : Quadbit::ComponentSystem {
	// Converts (flattens) a 3D index
	uint32_t ConvertIndex(uint32_t x, uint32_t y, uint32_t z) {
		return x + VOXEL_BLOCK_WIDTH * (y + VOXEL_BLOCK_WIDTH * z);
	}

	void AddVertex(glm::vec3 position, glm::vec3&& normal, glm::vec3 colour, std::vector<Quadbit::MeshVertex>& renderVertices, std::vector<PxVec3>& colliderVertices) {
		renderVertices.push_back({ position, normal, colour });
		colliderVertices.push_back(Quadbit::GlmVec3ToPx({ position }));
	}

	void AddQuad(const VisibleFaces face, std::vector<Quadbit::MeshVertex>& renderVertices, std::vector<PxVec3>& colliderVertices,
		std::vector<uint32_t>& indices, glm::float4x3 offsets, glm::vec3 colour) {
		switch(face) {
		case VisibleFaces::North:
			AddVertex(glm::vec3(1, 0, 1) + offsets[0], glm::vec3(0.0f, 0.0f, -1.0f), colour, renderVertices, colliderVertices);
			AddVertex(glm::vec3(1, 1, 1) + offsets[1], glm::vec3(0.0f, 0.0f, -1.0f), colour, renderVertices, colliderVertices);
			AddVertex(glm::vec3(0, 1, 1) + offsets[2], glm::vec3(0.0f, 0.0f, -1.0f), colour, renderVertices, colliderVertices);
			AddVertex(glm::vec3(0, 0, 1) + offsets[3], glm::vec3(0.0f, 0.0f, -1.0f), colour, renderVertices, colliderVertices);
			break;
		case VisibleFaces::South:
			AddVertex(glm::vec3(1, 0, 0) + offsets[0], glm::vec3(0.0f, 0.0f, 1.0f), colour, renderVertices, colliderVertices);
			AddVertex(glm::vec3(0, 0, 0) + offsets[1], glm::vec3(0.0f, 0.0f, 1.0f), colour, renderVertices, colliderVertices);
			AddVertex(glm::vec3(0, 1, 0) + offsets[2], glm::vec3(0.0f, 0.0f, 1.0f), colour, renderVertices, colliderVertices);
			AddVertex(glm::vec3(1, 1, 0) + offsets[3], glm::vec3(0.0f, 0.0f, 1.0f), colour, renderVertices, colliderVertices);
			break;
		case VisibleFaces::East:
			AddVertex(glm::vec3(1, 0, 0) + offsets[0], glm::vec3(1.0f, 0.0f, 0.0f), colour, renderVertices, colliderVertices);
			AddVertex(glm::vec3(1, 1, 0) + offsets[1], glm::vec3(1.0f, 0.0f, 0.0f), colour, renderVertices, colliderVertices);
			AddVertex(glm::vec3(1, 1, 1) + offsets[2], glm::vec3(1.0f, 0.0f, 0.0f), colour, renderVertices, colliderVertices);
			AddVertex(glm::vec3(1, 0, 1) + offsets[3], glm::vec3(1.0f, 0.0f, 0.0f), colour, renderVertices, colliderVertices);
			break;
		case VisibleFaces::West:
			AddVertex(glm::vec3(0, 0, 0) + offsets[0], glm::vec3(-1.0f, 0.0f, 0.0f), colour, renderVertices, colliderVertices);
			AddVertex(glm::vec3(0, 0, 1) + offsets[1], glm::vec3(-1.0f, 0.0f, 0.0f), colour, renderVertices, colliderVertices);
			AddVertex(glm::vec3(0, 1, 1) + offsets[2], glm::vec3(-1.0f, 0.0f, 0.0f), colour, renderVertices, colliderVertices);
			AddVertex(glm::vec3(0, 1, 0) + offsets[3], glm::vec3(-1.0f, 0.0f, 0.0f), colour, renderVertices, colliderVertices);
			break;
		case VisibleFaces::Top:
			AddVertex(glm::vec3(1, 1, 0) + offsets[0], glm::vec3(0.0f, 1.0f, 0.0f), colour, renderVertices, colliderVertices);
			AddVertex(glm::vec3(0, 1, 0) + offsets[1], glm::vec3(0.0f, 1.0f, 0.0f), colour, renderVertices, colliderVertices);
			AddVertex(glm::vec3(0, 1, 1) + offsets[2], glm::vec3(0.0f, 1.0f, 0.0f), colour, renderVertices, colliderVertices);
			AddVertex(glm::vec3(1, 1, 1) + offsets[3], glm::vec3(0.0f, 1.0f, 0.0f), colour, renderVertices, colliderVertices);
			break;
		case VisibleFaces::Bottom:
			AddVertex(glm::vec3(1, 0, 0) + offsets[0], glm::vec3(0.0f, -1.0f, 0.0f), colour, renderVertices, colliderVertices);
			AddVertex(glm::vec3(1, 0, 1) + offsets[1], glm::vec3(0.0f, -1.0f, 0.0f), colour, renderVertices, colliderVertices);
			AddVertex(glm::vec3(0, 0, 1) + offsets[2], glm::vec3(0.0f, -1.0f, 0.0f), colour, renderVertices, colliderVertices);
			AddVertex(glm::vec3(0, 0, 0) + offsets[3], glm::vec3(0.0f, -1.0f, 0.0f), colour, renderVertices, colliderVertices);
			break;
		}
		auto vertexCount = static_cast<uint32_t>(renderVertices.size());

		indices.push_back(vertexCount - 4 + 0);
		indices.push_back(vertexCount - 4 + 1);
		indices.push_back(vertexCount - 4 + 2);

		indices.push_back(vertexCount - 4 + 0);
		indices.push_back(vertexCount - 4 + 2);
		indices.push_back(vertexCount - 4 + 3);
	}

	void Update(float dt, Quadbit::QbVkRenderer* renderer) {
		auto entityManager = Quadbit::EntityManager::GetOrCreate();

		entityManager->ForEach<Quadbit::RenderTransformComponent, VoxelBlockComponent, MeshGenerationUpdateTag>
			([&](Quadbit::Entity entity, Quadbit::RenderTransformComponent& transform, VoxelBlockComponent& block, auto& tag) {

			std::vector<Quadbit::MeshVertex> renderVertices;
			std::vector<PxVec3> colliderVertices;
			std::vector<uint32_t> indices;
			renderVertices.reserve(VOXEL_BLOCK_SIZE);
			colliderVertices.reserve(VOXEL_BLOCK_SIZE);
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
							glm::vec3 colour = glm::vec3(0.1f + (heightCoeff * 0.2f), 0.3f - (heightCoeff * 0.2f), 0.05f);

							AddQuad(static_cast<VisibleFaces>(face), renderVertices, colliderVertices, indices, { offset, offset, offset, offset }, colour);
						}
					}
				}
			}

			auto physicsWorld = PhysicsWorld::GetOrCreate();

			PxTriangleMeshDesc triangleMeshDesc;
			triangleMeshDesc.points.count = static_cast<PxU32>(colliderVertices.size());
			triangleMeshDesc.points.stride = sizeof(PxVec3);
			triangleMeshDesc.points.data = colliderVertices.data();

			triangleMeshDesc.triangles.count = static_cast<PxU32>(indices.size() / 3);
			triangleMeshDesc.triangles.stride = 3 * sizeof(uint32_t);
			triangleMeshDesc.triangles.data = indices.data();

#ifdef QBDEBUG
			bool res = physicsWorld->cooking_->validateTriangleMesh(triangleMeshDesc);
			PX_ASSERT(res);
#endif

			PxRigidStatic* mesh = physicsWorld->physics_->createRigidStatic(PxTransform({ transform.position.x, transform.position.y, transform.position.z }));
			PxTriangleMesh* triangleMesh = physicsWorld->cooking_->createTriangleMesh(triangleMeshDesc, physicsWorld->physics_->getPhysicsInsertionCallback());
			PxTriangleMeshGeometry geometry(triangleMesh);
			auto mat = physicsWorld->physics_->createMaterial(0.0f, 0.0f, 0.0f);
			PxShape* shape = physicsWorld->physics_->createShape(geometry, *mat);
			mesh->attachShape(*shape);
			physicsWorld->scene_->addActor(*mesh);

			entity.AddComponent<Quadbit::RenderMeshComponent>(renderer->CreateMesh(renderVertices, indices));
			entity.AddComponent<Quadbit::StaticBodyComponent>({ mesh });
		});
	}
};
