#include <PCH.h>

#include "Water.h"

void Water::Init() {
	// Setup entities
	auto entityManager = Quadbit::EntityManager::GetOrCreate();

	std::vector<Quadbit::MeshVertex> vertices(512 * 512);
	std::vector<uint32_t> indices(511 * 511 * 6);

	int vertexCount = 0;
	int indexCount = 0;
	for (int x = 0; x < 512; x++) {
		for (int z = 0; z < 512; z++) {
			vertices[vertexCount] = { {x, 0, z}, {0.1f, 0.5f, 0.7f} };
			if (x < 512 - 1 && z < 512 - 1) {
				indices[indexCount++] = vertexCount;
				indices[indexCount++] = vertexCount + 512;
				indices[indexCount++] = vertexCount + 512 + 1;
				indices[indexCount++] = vertexCount;
				indices[indexCount++] = vertexCount + 512 + 1;
				indices[indexCount++] = vertexCount + 1;
			}
			vertexCount++;
		}
	}

	auto entity = entityManager->Create();
	entity.AddComponent<Quadbit::RenderMeshComponent>(renderer_->CreateMesh(vertices, indices));
	entity.AddComponent<Quadbit::RenderTransformComponent>(Quadbit::RenderTransformComponent(1.0f, { 0.0f, 0.0f, 0.0f }, { 0, 0, 0, 1 }));
}

void Water::Simulate(float deltaTime) {

}

void Water::DrawFrame() {
	renderer_->DrawFrame();
}