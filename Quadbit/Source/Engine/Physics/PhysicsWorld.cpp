#include <PCH.h>
#include "PhysicsWorld.h"

#include "PhysicsComponents.h"
#include "Systems/PositionUpdateSystem.h"
#include "../Entities/EntityManager.h"

namespace Quadbit {

	PhysicsWorld* PhysicsWorld::GetOrCreate() {
		static PhysicsWorld* instance = new PhysicsWorld();
		return instance;
	}

	void PhysicsWorld::StepSimulation(float dt) {
		scene_->simulate(dt);
		scene_->fetchResults(true);

		// Update positions of entities in world
		EntityManager::GetOrCreate()->systemDispatch_->RunSystem<PositionUpdateSystem>();
	}

	PxController* PhysicsWorld::CreateController(PxCapsuleControllerDesc desc) {
		return controllerManager_->createController(desc);
	}

	PhysicsWorld::PhysicsWorld() {
		foundation_ = PxCreateFoundation(PX_PHYSICS_VERSION, defaultAllocator_, defaultErrorCallback_);

		// Connect to the PhysX Visual Debugger
		visualDebugger_ = PxCreatePvd(*foundation_);
		PxPvdTransport* transport = PxDefaultPvdSocketTransportCreate("localhost", 5425, 100);
		visualDebugger_->connect(*transport, PxPvdInstrumentationFlag::eALL);

		// Create the main physics object
		physics_ = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation_, PxTolerancesScale(), true, visualDebugger_);
		if(!physics_) QB_LOG_ERROR("PxCreatePhysics failed!\n");

		// Create the cooker
		PxTolerancesScale scale;
		PxCookingParams params(scale);
		params.meshPreprocessParams |= PxMeshPreprocessingFlag::eDISABLE_CLEAN_MESH;
		params.meshPreprocessParams |= PxMeshPreprocessingFlag::eDISABLE_ACTIVE_EDGES_PRECOMPUTE;
		cooking_ = PxCreateCooking(PX_PHYSICS_VERSION, *foundation_, params);
		if(!cooking_) QB_LOG_ERROR("PxCreateCooking failed!\n");

		// Create main scene
		PxSceneDesc sceneDesc(physics_->getTolerancesScale());
		sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
		dispatcher_ = PxDefaultCpuDispatcherCreate(8);
		sceneDesc.cpuDispatcher = dispatcher_;
		sceneDesc.filterShader = PxDefaultSimulationFilterShader;
		scene_ = physics_->createScene(sceneDesc);

		PxPvdSceneClient* pvdClient = scene_->getScenePvdClient();
		if(pvdClient) {
			pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
			pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
			pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
		}

		controllerManager_ = PxCreateControllerManager(*scene_);

		EntityManager::GetOrCreate()->RegisterComponents<StaticBodyComponent, CharacterControllerComponent>();
	}

	PhysicsWorld::~PhysicsWorld() {
		controllerManager_->release();
		scene_->release();
		physics_->release();

		if(visualDebugger_ != nullptr) {
			PxPvdTransport* transport = visualDebugger_->getTransport();
			visualDebugger_->release();
			visualDebugger_ = nullptr;
		}

		foundation_->release();
	}
}