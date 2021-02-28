#include "stdafx.h"

#include "Headers/PhysX.h"
#include "Graphs/Headers/SceneGraphNode.h"
#include "Headers/PhysXSceneInterface.h"
#include "Platform/Video/Buffers/VertexBuffer/Headers/VertexBuffer.h"
#include "Utility/Headers/Localization.h"

// Connecting the SDK to Visual Debugger
#include <extensions/PxDefaultAllocator.h>
#include <pvd/PxPvd.h>

#include "ECS/Components/Headers/BoundsComponent.h"
#include "ECS/Components/Headers/RigidBodyComponent.h"
#include "ECS/Components/Headers/TransformComponent.h"
#include "Environment/Terrain/Headers/Terrain.h"
#include "Geometry/Shapes/Headers/Object3D.h"
#include "Platform/File/Headers/FileManagement.h"
// PhysX includes //

namespace Divide {

namespace {
    constexpr bool g_recordMemoryAllocations = false;
    const char* g_collisionMeshExtension = "DVDColMesh";

    const char* g_pvd_target_ip = "127.0.0.1";
    physx::PxU32 g_pvd_target_port = 5425;
    physx::PxU32 g_pvd_target_timeout_ms = 10;

    struct DeletionListener final : physx::PxDeletionListener {
        void onRelease(const physx::PxBase* observed, void* userData, physx::PxDeletionEventFlag::Enum deletionEvent) override {
            PX_UNUSED(userData);
            PX_UNUSED(deletionEvent);

            if (observed->is<physx::PxRigidActor>()) 	{
                const physx::PxRigidActor* actor = static_cast<const physx::PxRigidActor*>(observed);
                PX_UNUSED(actor);
                /*
                removeRenderActorsFromPhysicsActor(actor);
                vectorEASTL<physx::PxRigidActor*>::iterator actorIter = std::find(_physicsActors.begin(), _physicsActors.end(), actor);
                if (actorIter != _physicsActors.end()) {
                    _physicsActors.erase(actorIter);
                }
                */

            }
        }
    } g_deletionListener;

    struct DvdErrorCallback final : physx::PxErrorCallback
    {
        void reportError(const physx::PxErrorCode::Enum code, const char* message, const char* file, const int line) override {
            switch(code) {
                case physx::PxErrorCode::eNO_ERROR: Console::printfn(Locale::Get(_ID("ERROR_PHYSX_GENERIC")), "None", message, file, line);  return;
                case physx::PxErrorCode::eDEBUG_INFO:  Console::d_printfn(Locale::Get(_ID("ERROR_PHYSX_GENERIC")), "Debug Msg", message, file, line); return;
                case physx::PxErrorCode::eDEBUG_WARNING:  Console::d_warnf(Locale::Get(_ID("ERROR_PHYSX_GENERIC")), "Debug Warn", message, file, line); return;
                case physx::PxErrorCode::eINVALID_PARAMETER:  Console::errorfn(Locale::Get(_ID("ERROR_PHYSX_GENERIC")), "Invalid Parameter", message, file, line); return;
                case physx::PxErrorCode::eINVALID_OPERATION:  Console::errorfn(Locale::Get(_ID("ERROR_PHYSX_GENERIC")), "Invalid Operation", message, file, line); return;
                case physx::PxErrorCode::eOUT_OF_MEMORY:  Console::errorfn(Locale::Get(_ID("ERROR_PHYSX_GENERIC")), "Mem", message, file, line); return;
                case physx::PxErrorCode::eINTERNAL_ERROR:  Console::errorfn(Locale::Get(_ID("ERROR_PHYSX_GENERIC")), "Internal", message, file, line); return;
                case physx::PxErrorCode::eABORT:  Console::errorfn(Locale::Get(_ID("ERROR_PHYSX_GENERIC")), "Abort", message, file, line); return;
                case physx::PxErrorCode::ePERF_WARNING:  Console::warnfn(Locale::Get(_ID("ERROR_PHYSX_GENERIC")), "Perf", message, file, line); return;
                case physx::PxErrorCode::eMASK_ALL:  Console::errorfn(Locale::Get(_ID("ERROR_PHYSX_GENERIC")), "ALL", message, file, line); return;
            }
            Console::errorfn(Locale::Get(_ID("ERROR_PHYSX_GENERIC")), "UNKNOWN", message, file, line);
        }
    } g_physxErrorCallback;

};

physx::PxDefaultAllocator PhysX::_gDefaultAllocatorCallback;
hashMap<U64, physx::PxTriangleMesh*> PhysX::s_gMeshCache;
SharedMutex PhysX::s_meshCacheLock;

PhysX::~PhysX()
{
    assert(_gPhysicsSDK == nullptr);
}

ErrorCode PhysX::initPhysicsAPI(const U8 targetFrameRate, const F32 simSpeed) {
    Console::printfn(Locale::Get(_ID("START_PHYSX_API")));

    _simulationSpeed = simSpeed;
    // create foundation object with default error and allocator callbacks.
    _foundation = PxCreateFoundation(PX_PHYSICS_VERSION, _gDefaultAllocatorCallback, g_physxErrorCallback);
    assert(_foundation != nullptr);
#if PX_SUPPORT_GPU_PHYSX
    physx::PxCudaContextManagerDesc cudaContextManagerDesc;
    cudaContextManagerDesc.interopMode = physx::PxCudaInteropMode::NO_INTEROP;
    _cudaContextManager = PxCreateCudaContextManager(*_foundation, cudaContextManagerDesc, PxGetProfilerCallback());
    if (_cudaContextManager != nullptr) {
        if (!_cudaContextManager->contextIsValid()) {
            _cudaContextManager->release();
            _cudaContextManager = nullptr;
        }
    }
#endif //PX_SUPPORT_GPU_PHYSX
    if_constexpr(Config::Build::IS_DEBUG_BUILD || Config::Build::IS_PROFILE_BUILD) {
        createPvdConnection(g_pvd_target_ip,
                            g_pvd_target_port,
                            g_pvd_target_timeout_ms,
                            Config::Build::IS_DEBUG_BUILD);
    }

    physx::PxTolerancesScale toleranceScale{};
    _gPhysicsSDK = PxCreatePhysics(PX_PHYSICS_VERSION,  *_foundation, toleranceScale, g_recordMemoryAllocations, _pvd);

    if (_gPhysicsSDK == nullptr) {
        closePhysicsAPI();

        Console::errorfn(Locale::Get(_ID("ERROR_START_PHYSX_API")));
        return ErrorCode::PHYSX_INIT_ERROR;
    }

    if (!PxInitExtensions(*_gPhysicsSDK, _pvd)) {
        closePhysicsAPI();

        Console::errorfn(Locale::Get(_ID("ERROR_EXTENSION_PHYSX_API")));
        return ErrorCode::PHYSX_EXTENSION_ERROR;
    }

    if (!_cooking) {
        physx::PxCookingParams params(toleranceScale);
        params.meshWeldTolerance = 0.001f;
        params.meshPreprocessParams = physx::PxMeshPreprocessingFlags(physx::PxMeshPreprocessingFlag::eWELD_VERTICES);
        params.buildGPUData = true; //Enable GRB data being produced in cooking.

        _cooking = PxCreateCooking(PX_PHYSICS_VERSION, *_foundation, params);
    }
    if (_cooking == nullptr) {
        closePhysicsAPI();
        Console::errorfn(Locale::Get(_ID("ERROR_START_PHYSX_API")));
        return ErrorCode::PHYSX_INIT_ERROR;
    }

    _gPhysicsSDK->registerDeletionListener(g_deletionListener, physx::PxDeletionEventFlag::eUSER_RELEASE);

    //ToDo: Add proper material controls to RigidBodyComponent -Ionut
    _defaultMaterial = _gPhysicsSDK->createMaterial(0.5f, 0.5f, 0.1f);
    if (_defaultMaterial == nullptr) {
        closePhysicsAPI();

        Console::errorfn(Locale::Get(_ID("ERROR_START_PHYSX_API")));
        return ErrorCode::PHYSX_INIT_ERROR;
    }

    updateTimeStep(targetFrameRate, _simulationSpeed);
    Console::printfn(Locale::Get(_ID("START_PHYSX_API_OK")));

    return ErrorCode::NO_ERR;
}

#define SAFE_RELEASE(X) if (X != nullptr) { X->release(); X = nullptr;}

bool PhysX::closePhysicsAPI() {
    if (!_gPhysicsSDK) {
        return false;
    }

    Console::printfn(Locale::Get(_ID("STOP_PHYSX_API")));

    DIVIDE_ASSERT(_targetScene == nullptr, "PhysX error: target scene not destroyed before calling closePhysicsAPI.");

    SAFE_RELEASE(_gPhysicsSDK);
    SAFE_RELEASE(_cooking);
    PxCloseExtensions();
    SAFE_RELEASE(_cudaContextManager);
    SAFE_RELEASE(_pvd);
    SAFE_RELEASE(_transport);
    SAFE_RELEASE(_foundation);

    return true;
}

void PhysX::togglePvdConnection() const {
    if (_pvd == nullptr) {
        return;
    }

    if (_pvd->isConnected()) {
        _pvd->disconnect();
    } else {
        if (_pvd->connect(*_transport, _pvdFlags)) {
            Console::d_printfn(Locale::Get(_ID("CONNECT_PVD_OK")));
        }
    }
}

void PhysX::createPvdConnection(const char* ip, const physx::PxU32 port, const physx::PxU32 timeout, const bool useFullConnection) {
    //Create a pvd connection that writes data straight to the filesystem.  This is
    //the fastest connection on windows for various reasons.  First, the transport is quite fast as
    //pvd writes data in blocks and filesystems work well with that abstraction.
    //Second, you don't have the PVD application parsing data and using CPU and memory bandwidth
    //while your application is running.
    //physx::PxPvdTransport* transport = physx::PxDefaultPvdFileTransportCreate( "c:\\mywork\\sample.pxd2" );

    //The normal way to connect to pvd.  PVD needs to be running at the time this function is called.
    //We don't worry about the return value because we are already registered as a listener for connections
    //and thus our onPvdConnected call will take care of setting up our basic connection state.
    _transport = physx::PxDefaultPvdSocketTransportCreate(ip, port, timeout);
    if (_transport == nullptr) {
        return;
    }

    //The connection flags state overall what data is to be sent to PVD.  Currently
    //the Debug connection flag requires support from the implementation (don't send
    //the data when debug isn't set) but the other two flags, profile and memory
    //are taken care of by the PVD SDK.

    //Use these flags for a clean profile trace with minimal overhead
    _pvdFlags = physx::PxPvdInstrumentationFlag::eALL;
    if (!useFullConnection)
    {
        _pvdFlags = physx::PxPvdInstrumentationFlag::ePROFILE;
    }
    _pvd = physx::PxCreatePvd(*_foundation);
    //if (_pvd->connect(*_transport, _pvdFlags)) {
    //    Console::d_printfn(Locale::Get(_ID("CONNECT_PVD_OK")));
    //}
}

inline void PhysX::updateTimeStep(U8 timeStepFactor, const F32 simSpeed) {
    timeStepFactor = to_U8(std::max(1u, timeStepFactor * 1u));
    _timeStepFactor = timeStepFactor;
    _timeStep = 1.0f / _timeStepFactor;
    _timeStep *= simSpeed;
}

/// Process results
void PhysX::process(const U64 deltaTimeUS) {
    if (_targetScene && _timeStep > 0.0f) {
        _accumulator +=
            Time::MicrosecondsToMilliseconds<physx::PxReal>(deltaTimeUS);

        if (_accumulator < _timeStep) {
            return;
        }

        _accumulator -= _timeStep;
        _targetScene->process(Time::SecondsToMicroseconds(_timeStep));
    }
}

/// Update actors
void PhysX::update(const U64 deltaTimeUS) {
    if (_targetScene != nullptr) {
        _targetScene->update(deltaTimeUS);
    }
}

void PhysX::idle() {
    if (_targetScene != nullptr) {
        _targetScene->idle();
    }
}

bool PhysX::initPhysicsScene(Scene& scene) {
    if (_targetScene != nullptr) {
        destroyPhysicsScene();
    }

    DIVIDE_ASSERT(_targetScene == nullptr);
    _targetScene = eastl::make_unique<PhysXSceneInterface>(scene);
    if (_targetScene != nullptr) {
        _targetScene->init();
        return true;
    }

    return false;
}

bool PhysX::destroyPhysicsScene() { 
    if (_targetScene != nullptr) {
        /// Destroy physics (:D)
        _targetScene->release();
        _targetScene.reset();
    }

    return true;
}

PhysicsAsset* PhysX::createRigidActor(SceneGraphNode* node, RigidBodyComponent& parentComp)
{
    PhysXActor* newActor = new PhysXActor(parentComp);

    TransformComponent* tComp = node->get<TransformComponent>();
    assert(tComp != nullptr);

    const vec3<F32>& position = tComp->getPosition();
    const vec4<F32>& orientation = tComp->getOrientation().asVec4();
    const physx::PxTransform posePxTransform(physx::PxVec3(position.x, position.y, position.z),
        physx::PxQuat(orientation.x,
                      orientation.y,
                      orientation.z,
                      orientation.w).getConjugate());
    //if (parentComp.physicsCollisionGroup() == PhysicsGroup::GROUP_STATIC) {
        newActor->_actor = _gPhysicsSDK->createRigidStatic(posePxTransform);
    /*} else if (parentComp.physicsCollisionGroup() == PhysicsGroup::GROUP_DYNAMIC) {
        newActor->_actor = _gPhysicsSDK->createRigidDynamic(posePxTransform);
    } else {
        DIVIDE_UNEXPECTED_CALL();
    }*/

    if (!newActor->_actor) {
        MemoryManager::DELETE(newActor);
        return nullptr;
    }
    newActor->_actor->userData = node;

    SceneNode& sNode = node->getNode();
    const stringImpl meshName = sNode.assetName().empty() ? sNode.resourceName().c_str() : sNode.assetName().str();
    const U64 nameHash = _ID(meshName.c_str());

    const ResourcePath collMeshPath = Paths::g_cacheLocation + Paths::g_collisionMeshCacheLocation;
    const ResourcePath cachePath = collMeshPath + meshName + "." + g_collisionMeshExtension;

    if (sNode.type() == SceneNodeType::TYPE_OBJECT3D) {
        newActor->_type = physx::PxGeometryType::eTRIANGLEMESH;

        physx::PxTriangleMesh* nodeGeometry = nullptr;
        {
            SharedLock<SharedMutex> r_lock(s_meshCacheLock);
            const auto it = s_gMeshCache.find(nameHash);
            if (it != s_gMeshCache.end()) {
                nodeGeometry = it->second;
                Console::printfn(Locale::Get(_ID("COLLISION_MESH_LOADED_FROM_RAM")), meshName.c_str());
            }
        }

        if (nodeGeometry == nullptr) {
            UniqueLock<SharedMutex> w_lock(s_meshCacheLock);
            // Check again to avoid race conditions
            const auto it = s_gMeshCache.find(nameHash);
            if (it != s_gMeshCache.end()) {
                nodeGeometry = it->second;
                Console::printfn(Locale::Get(_ID("COLLISION_MESH_LOADED_FROM_RAM")), meshName.c_str());
            } else {
                Object3D& obj = node->getNode<Object3D>();
                const U8 lodCount = obj.getGeometryPartitionCount();
                const U16 partitionID = obj.getGeometryPartitionID(lodCount - 1);
                obj.computeTriangleList(partitionID);
                const vectorEASTL<vec3<U32>>& triangles = obj.getTriangles(partitionID);

                if (triangles.empty()) {
                    MemoryManager::DELETE(newActor);
                    return nullptr;
                }

                const bool collisionMeshFileExists = fileExists(cachePath);
                if (!collisionMeshFileExists && !pathExists(collMeshPath)) {
                    if (!createDirectory(collMeshPath)) {
                        DIVIDE_UNEXPECTED_CALL();
                    }
                }
                if (!collisionMeshFileExists) {
                    physx::PxTriangleMeshDesc meshDesc;
                    meshDesc.triangles.count = static_cast<physx::PxU32>(triangles.size());
                    meshDesc.triangles.stride = 3 * sizeof(U32);
                    meshDesc.triangles.data = triangles.data();

                    meshDesc.points.stride = sizeof(VertexBuffer::Vertex);
                    physx::PxDefaultFileOutputStream outputStream(cachePath.c_str());
                    if (obj.getObjectType()._value == ObjectType::TERRAIN) {
                        const vectorEASTL<VertexBuffer::Vertex>& verts = node->getNode<Terrain>().getVerts();
                        meshDesc.points.count = static_cast<physx::PxU32>(verts.size());
                        meshDesc.points.data = verts[0]._position._v;
                    } else {
                        VertexBuffer* vb = obj.getGeometryVB();
                        DIVIDE_ASSERT(vb != nullptr);
                        meshDesc.points.count = static_cast<physx::PxU32>(vb->getVertexCount());
                        meshDesc.points.stride = sizeof(VertexBuffer::Vertex);
                        meshDesc.points.data = vb->getVertices()[0]._position._v;
                    }
                    
                    const auto getErrorMessage = [](const physx::PxTriangleMeshCookingResult::Enum value) {
                        switch (value) {
                        case physx::PxTriangleMeshCookingResult::Enum::eSUCCESS:
                            return "Success";
                        case physx::PxTriangleMeshCookingResult::Enum::eLARGE_TRIANGLE:
                            return "A triangle is too large for well-conditioned results. Tessellate the mesh for better behavior, see the user guide section on cooking for more details.";
                        case physx::PxTriangleMeshCookingResult::Enum::eFAILURE:
                            return "Something unrecoverable happened. Check the error stream to find out what.";
                        }

                        return "UNKNWOWN";
                    };

                    physx::PxTriangleMeshCookingResult::Enum result;
                    if (!_cooking->cookTriangleMesh(meshDesc, outputStream, &result)) {
                        STUBBED("ToDo: If we fail to build/load a collision mesh, fallback to an AABB aproximation -Ionut")
                        Console::errorfn(Locale::Get(_ID("ERROR_COOK_TRIANGLE_MESH")), getErrorMessage(result));
                    }
                } else  {
                    Console::printfn(Locale::Get(_ID("COLLISION_MESH_LOADED_FROM_FILE")), meshName.c_str());
                }

                physx::PxDefaultFileInputData inData(cachePath.c_str());
                nodeGeometry = _gPhysicsSDK->createTriangleMesh(inData);
                if (nodeGeometry) {
                    hashAlg::insert(s_gMeshCache, nameHash, nodeGeometry);
                } else {
                    Console::errorfn(Locale::Get(_ID("ERROR_CREATE_TRIANGLE_MESH")));
                }
            }
        }
        if (nodeGeometry != nullptr) {
            const vec3<F32>& scale = tComp->getScale();
            physx::PxTriangleMeshGeometry geometry = {
                nodeGeometry,
                physx::PxMeshScale(physx::PxVec3(scale.x, scale.y, scale.z),
                                   physx::PxQuat(physx::PxIdentity))
            };

            STUBBED("PhysX implementation only uses one shape per actor for now! -Ionut")
            physx::PxRigidActorExt::createExclusiveShape(*newActor->_actor, geometry, *_defaultMaterial);

        }
    } else if (sNode.type() == SceneNodeType::TYPE_INFINITEPLANE ||
               sNode.type() == SceneNodeType::TYPE_WATER ||
               sNode.type() == SceneNodeType::TYPE_TRIGGER ||
               sNode.type() == SceneNodeType::TYPE_PARTICLE_EMITTER) {
        // Use AABB
        BoundsComponent* bComp = node->get<BoundsComponent>();
        assert(bComp != nullptr);
        const vec3<F32> hExtent = bComp->getBoundingBox().getHalfExtent();

        newActor->_type = physx::PxGeometryType::eBOX;

        physx::PxBoxGeometry geometry = { hExtent.x, hExtent.y, hExtent.z};
        physx::PxShape* shape = physx::PxRigidActorExt::createExclusiveShape(*newActor->_actor, geometry, *_defaultMaterial);
        PX_UNUSED(shape);
    } else {
        DIVIDE_UNEXPECTED_CALL();
    }

    // If we got here, the new actor was just created (didn't exist previously in the scene), so add it
    _targetScene->addRigidActor(newActor);

    return newActor;
}

bool PhysX::convertActor(PhysicsAsset* actor, const PhysicsGroup newGroup) {
    PhysXActor* targetActor = dynamic_cast<PhysXActor*>(actor);

    if (targetActor != nullptr) {
        physx::PxRigidActor* tempActor = nullptr;
        if (newGroup == PhysicsGroup::GROUP_STATIC) {
            tempActor = _gPhysicsSDK->createRigidStatic(targetActor->_actor->getGlobalPose());
        } else if (newGroup == PhysicsGroup::GROUP_DYNAMIC) {
            tempActor = _gPhysicsSDK->createRigidDynamic(targetActor->_actor->getGlobalPose());
        } else {
            DIVIDE_UNEXPECTED_CALL();
        }

        const physx::PxU32 nShapes = targetActor->_actor->getNbShapes();
        vectorEASTL<physx::PxShape*> shapes(nShapes);
        targetActor->_actor->getShapes(shapes.data(), nShapes);
        for (physx::PxU32 i = 0; i < nShapes; ++i) {
            tempActor->attachShape(*shapes[i]);
        }

        _targetScene->updateRigidActor(targetActor->_actor, tempActor);
        targetActor->_actor = tempActor;
        return true;
    }

    return false;
}

};
