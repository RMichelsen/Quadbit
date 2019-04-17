#include <PCH.h>
#include "EntityManager.h"

namespace Quadbit {
	Entity EntityManager::Create() {
		return Entity(EntityID(nextEntityId_++, 1));
	}
}