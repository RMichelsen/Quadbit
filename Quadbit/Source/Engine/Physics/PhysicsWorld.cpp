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

	PxRigidStatic* PhysicsWorld::CreateTriangleMesh(PxTransform physxTransform, const std::vector<PxVec3>& colliderVertices, const std::vector<uint32_t>& indices) {
		auto params = cooking_->getParams();

		PxTriangleMeshDesc triangleMeshDesc;
		triangleMeshDesc.points.count = static_cast<PxU32>(colliderVertices.size());
		triangleMeshDesc.points.stride = sizeof(PxVec3);
		triangleMeshDesc.points.data = colliderVertices.data();

		triangleMeshDesc.triangles.count = static_cast<PxU32>(indices.size() / 3);
		triangleMeshDesc.triangles.stride = 3 * sizeof(uint32_t);
		triangleMeshDesc.triangles.data = indices.data();

#ifdef QBDEBUG
		bool res = cooking_->validateTriangleMesh(triangleMeshDesc);
		PX_ASSERT(res);
#endif

		PxRigidStatic* mesh = physics_->createRigidStatic(physxTransform);
		PxTriangleMesh* triangleMesh = cooking_->createTriangleMesh(triangleMeshDesc, physics_->getPhysicsInsertionCallback());
		PxTriangleMeshGeometry geometry(triangleMesh);
		PxShape* shape = physics_->createShape(geometry, *defaultMaterial_);
		mesh->attachShape(*shape);
		scene_->addActor(*mesh);
		
		return mesh;
	}

	PxRigidStatic* PhysicsWorld::CreateBoxCollider(PxTransform physxTransform) {
		PxRigidStatic* boxMesh = physics_->createRigidStatic(physxTransform);
		PxShape* shape = PxRigidActorExt::createExclusiveShape(*boxMesh, PxBoxGeometry(0.5f, 0.5f, 0.5f), *defaultMaterial_);
		boxMesh->attachShape(*shape);
		scene_->addActor(*boxMesh);

		return boxMesh;
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

		// Parameters to faciliate fast cooking at the expense of mesh quality
		// TODO: Future idea might be to cook individual voxels as cube meshes around player
		params.meshPreprocessParams |= PxMeshPreprocessingFlag::eDISABLE_CLEAN_MESH;
		params.meshPreprocessParams |= PxMeshPreprocessingFlag::eDISABLE_ACTIVE_EDGES_PRECOMPUTE;
		params.suppressTriangleMeshRemapTable = true;
		params.midphaseDesc.setToDefault(PxMeshMidPhase::eBVH34);
		params.midphaseDesc.mBVH34Desc.numPrimsPerLeaf = 15;

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

		defaultMaterial_ = physics_->createMaterial(0.0f, 0.0f, 0.0f);

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