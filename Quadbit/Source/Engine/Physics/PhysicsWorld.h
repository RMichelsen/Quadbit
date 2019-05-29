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
		PxRigidStatic* CreateTriangleMesh(PxTransform physxTransform, const std::vector<PxVec3>& colliderVertices, const std::vector<uint32_t>& indices);
		PxRigidStatic* CreateBoxCollider(PxTransform physxTransform);

	private:
		PxDefaultErrorCallback defaultErrorCallback_;
		PxDefaultAllocator defaultAllocator_;

		PxControllerManager* controllerManager_;
		PxFoundation* foundation_;
		PxCpuDispatcher* dispatcher_;
		PxPvd* visualDebugger_;

		PxMaterial* defaultMaterial_;

		// Delete copy constructor and assignment operator
		PhysicsWorld(const PhysicsWorld&) = delete;
		PhysicsWorld& operator= (const PhysicsWorld&) = delete;
	};
}