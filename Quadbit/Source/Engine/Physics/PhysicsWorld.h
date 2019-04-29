#pragma once

namespace Quadbit {
	class PhysicsWorld {
	public:
		PhysicsWorld();
		~PhysicsWorld();

		static PhysicsWorld* GetOrCreate();

	private:
		btDefaultCollisionConfiguration* collisionConfiguration_;
		btCollisionDispatcher* dispatcher_;
		btBroadphaseInterface* overlappingPairCache_;
		btSequentialImpulseConstraintSolver* solver_;
		btDiscreteDynamicsWorld* dynamicsWorld_;

		// Delete copy constructor and assignment operator
		PhysicsWorld(const PhysicsWorld&) = delete;
		PhysicsWorld& operator= (const PhysicsWorld&) = delete;
	};
}