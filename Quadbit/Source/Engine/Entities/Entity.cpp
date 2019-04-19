#include <PCH.h>
#include "Entity.h"

#include "EntityManager.h"

namespace Quadbit {
	void Entity::Destroy() {
		manager_->Destroy(*this);
	}

	bool Entity::IsValid() {
		return id_.version == manager_->entityVersions_[id_.version];
	}
}