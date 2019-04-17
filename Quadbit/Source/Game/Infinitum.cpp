#include <PCH.h>
#include "Infinitum.h"
#include "../Engine/Entities/EntityManager.h"
#include "../Engine/Entities/Entity.h"
#include "../Engine/Entities/Component.h"
#include "../Engine/DataStructures/SparseSet.h"

struct validComponent {
	float xyz;
	float xyza;
	float xyzb;
	float xyzc;
	float xyzd;
};

struct invalidComponent {
	float xyz;
};

void Infinitum::Init() {
	Quadbit::EntityManager entityManager;
	
	entityManager.RegisterComponent<validComponent>();
	entityManager.RegisterComponent<validComponent>();

	auto ptr = std::static_pointer_cast<Quadbit::SparseSet<validComponent>>(entityManager.componentPools_[0]);
	ptr->dense_.push_back(validComponent{ 0,1,2,3,4 });

}

void Infinitum::Simulate(float deltaTime) {
}
