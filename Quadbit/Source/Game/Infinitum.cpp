#include <PCH.h>
#include "Infinitum.h"

#include "../Engine/Application/Time.h"
#include "../Engine/Entities/InternalTypes.h"

void Infinitum::Init() {
	//entityManager_->RegisterComponent<Quadbit::RenderMesh>();


	for(auto i = 0; i < 20; i++) {
		for(auto j = 0; j < 20; j++) {
			for(auto k = 0; k < 20; k++) {
				auto entity = entityManager_->Create();
				entity.AddComponent<Quadbit::RenderMesh>(
					renderer_->CreateMesh(
						Quadbit::cubeVertices, Quadbit::cubeIndices, 1.0f, 
						glm::vec3(static_cast<float>(i * 4), static_cast<float>(j * 4), static_cast<float>(k * 4)), 
						glm::quat())
					);
			}
		}
	}

	//for(auto i = 0; i < 1000; i++) {
	//	auto entity = entityManager_->Create();
	//	entity.AddComponent<Quadbit::Mesh>({ 0, 0, glm::mat4() });
	//	entity.Destroy();
	//}
}

void Infinitum::Simulate(float deltaTime) {
	//entityManager_->ForEach<Quadbit::Mesh>([](Quadbit::Mesh& mesh) {
	//	mesh.indexHandle++;
	//});
}

void Infinitum::DrawFrame() {
	renderer_->DrawFrame();
}