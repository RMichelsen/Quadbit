#include <PCH.h>
#include "PhysicsWorld.h"

namespace Quadbit {

	PhysicsWorld* PhysicsWorld::GetOrCreate() {
		static PhysicsWorld* instance = new PhysicsWorld();
		return instance;
	}

	PhysicsWorld::PhysicsWorld() {
		// Collision configuration contains default memory and collision setup, TODO: Customize?
		collisionConfiguration_ = new btDefaultCollisionConfiguration();

		dispatcher_ = new btCollisionDispatcher(collisionConfiguration_);

		overlappingPairCache_ = new btDbvtBroadphase();

		solver_ = new btSequentialImpulseConstraintSolver();

		dynamicsWorld_ = new btDiscreteDynamicsWorld(dispatcher_, overlappingPairCache_, solver_, collisionConfiguration_);

		dynamicsWorld_->setGravity(btVector3(0, -10, 0));
	}

	PhysicsWorld::~PhysicsWorld() {
		// Remove rigidbodies in reverse order of creation

		auto collisionObjects = dynamicsWorld_->getCollisionObjectArray();
		for(auto i = dynamicsWorld_->getNumCollisionObjects() - 1; i >= 0; i--) {
			btCollisionObject* object = collisionObjects[i];
			btRigidBody* body = btRigidBody::upcast(object);
			if(body && body->getMotionState()) delete body->getMotionState();
			dynamicsWorld_->removeCollisionObject(object);
			delete object;
		}

		delete dynamicsWorld_;
		delete solver_;
		delete overlappingPairCache_;
		delete dispatcher_;
		delete collisionConfiguration_;
	}
}