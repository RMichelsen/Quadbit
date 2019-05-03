#pragma once

using namespace physx;

namespace Quadbit {
	class PhysicsWorld {
	public:
		PxCooking* cooking_;
		PxPhysics* physics_;
		PxScene* scene_;

		PhysicsWorld();
		~PhysicsWorld();

		static PhysicsWorld* GetOrCreate();
		void StepSimulation(float dt);
		PxController* CreateController(PxCapsuleControllerDesc desc);

	private:
		PxDefaultErrorCallback defaultErrorCallback_;
		PxDefaultAllocator defaultAllocator_;

		PxControllerManager* controllerManager_;
		PxFoundation* foundation_;
		PxCpuDispatcher* dispatcher_;
		PxPvd* visualDebugger_;

		// Delete copy constructor and assignment operator
		PhysicsWorld(const PhysicsWorld&) = delete;
		PhysicsWorld& operator= (const PhysicsWorld&) = delete;
	};
}