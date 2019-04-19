#include <PCH.h>
#include "Infinitum.h"

#include "../Engine/Entities/InternalTypes.h"

struct Position {
	float x;
	float y;
	float z;
};

struct Rotation {
	glm::vec3 rot;
};

void Infinitum::Init() {
	entityManager_->RegisterComponent<Position>();
	entityManager_->RegisterComponent<Rotation>();

	for(auto i = 0; i < 1000000; i++) {
		auto entity = entityManager_->Create();
		entity.AddComponent<Position>();
	}
}

void Infinitum::Simulate(float deltaTime) {
	entityManager_->ParForEach<Position>([](auto & pos) {
		pos.x += std::sqrt(std::sin(1.0f) * std::cos(1.0f));
	});
}

void Infinitum::DrawFrame() {
	renderer_->DrawFrame();
}