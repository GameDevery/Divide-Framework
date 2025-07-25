#include "Headers/Jolt.h"

// Jolt includes
// Disable common warnings triggered by Jolt, you can use JPH_SUPPRESS_WARNING_PUSH / JPH_SUPPRESS_WARNING_POP to store and restore the warning state
JPH_SUPPRESS_WARNING_PUSH
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
JPH_SUPPRESS_WARNING_POP

static void TraceImpl(const char* inFMT, ...)
{
    // Format the message
    va_list list;
    va_start(list, inFMT);
    char buffer[2048];
    vsnprintf(buffer, sizeof(buffer) - 1, inFMT, list);
    va_end(list);

    Divide::Console::printfn("JoltPhysics: {}", buffer);
}

#ifdef JPH_ENABLE_ASSERTS

// Callback for asserts, connect this to your own assert handler if you have one
static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, JPH::uint inLine)
{
    Divide::DIVIDE_UNEXPECTED_CALL_MSG(Divide::Util::StringFormat("[{}:{}] : ( {} ) - {}", inFile, inLine, inExpression, (inMessage != nullptr ? inMessage : "")).c_str());
    return true;
}

#endif // JPH_ENABLE_ASSERTS

namespace Divide
{

    // Layer that objects can be in, determines which other objects it can collide with
    // Typically you at least want to have 1 layer for moving bodies and 1 layer for static bodies, but you can have more
    // layers if you want. E.g. you could have a layer for high detail collision (which is not used by the physics simulation
    // but only if you do collision testing).
    namespace Layers
    {
        static constexpr JPH::ObjectLayer NON_MOVING = 0;
        static constexpr JPH::ObjectLayer MOVING = 1;
        static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
    };

    /// Class that determines if two object layers can collide
    class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
    {
    public:
        virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
        {
            switch (inObject1)
            {
            case Layers::NON_MOVING:
                return inObject2 == Layers::MOVING; // Non moving only collides with moving
            case Layers::MOVING:
                return true; // Moving collides with everything
            default:
                JPH_ASSERT(false);
                return false;
            }
        }
    };

    namespace BroadPhaseLayers
    {
        static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
        static constexpr JPH::BroadPhaseLayer MOVING(1);
        static constexpr JPH::uint NUM_LAYERS(2);
    };

    // BroadPhaseLayerInterface implementation
    // This defines a mapping between object and broadphase layers.
    class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
    {
    public:
        BPLayerInterfaceImpl()
        {
            // Create a mapping table from object to broad phase layer
            mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
            mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
        }

        virtual JPH::uint GetNumBroadPhaseLayers() const override
        {
            return BroadPhaseLayers::NUM_LAYERS;
        }

        virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
        {
            JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
            return mObjectToBroadPhase[inLayer];
        }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
        {
            switch ((JPH::BroadPhaseLayer::Type)inLayer)
            {
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:	return "NON_MOVING";
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:		return "MOVING";
            default:													JPH_ASSERT(false); return "INVALID";
            }
        }
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

    private:
        JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
    };

    /// Class that determines if an object layer can collide with a broadphase layer
    class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
    {
    public:
        virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
        {
            switch (inLayer1)
            {
            case Layers::NON_MOVING:
                return inLayer2 == BroadPhaseLayers::MOVING;
            case Layers::MOVING:
                return true;
            default:
                JPH_ASSERT(false);
                return false;
            }
        }
    };

    // An example contact listener
    class MyContactListener : public JPH::ContactListener
    {
    public:
        // See: ContactListener
        virtual JPH::ValidateResult	OnContactValidate([[maybe_unused]] const JPH::Body& inBody1, [[maybe_unused]] const JPH::Body& inBody2, [[maybe_unused]] JPH::RVec3Arg inBaseOffset, [[maybe_unused]] const JPH::CollideShapeResult& inCollisionResult) override
        {
            Console::printfn("Contact validate callback");

            // Allows you to ignore a contact before it is created (using layers to not make objects collide is cheaper!)
            return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
        }

        virtual void OnContactAdded([[maybe_unused]] const JPH::Body& inBody1, [[maybe_unused]] const JPH::Body& inBody2, [[maybe_unused]] const JPH::ContactManifold& inManifold, [[maybe_unused]] JPH::ContactSettings& ioSettings) override
        {
            Console::printfn("A contact was added");
        }

        virtual void OnContactPersisted([[maybe_unused]] const JPH::Body& inBody1, [[maybe_unused]] const JPH::Body& inBody2, [[maybe_unused]] const JPH::ContactManifold& inManifold, [[maybe_unused]] JPH::ContactSettings& ioSettings) override
        {
            Console::printfn("A contact was persisted");
        }

        virtual void OnContactRemoved([[maybe_unused]] const JPH::SubShapeIDPair& inSubShapePair) override
        {
            Console::printfn("A contact was removed");
        }
    };

    // An example activation listener
    class MyBodyActivationListener : public JPH::BodyActivationListener
    {
    public:
        virtual void OnBodyActivated([[maybe_unused]] const JPH::BodyID& inBodyID, [[maybe_unused]] JPH::uint64 inBodyUserData) override
        {
            Console::printfn("A body got activated");
        }

        virtual void OnBodyDeactivated([[maybe_unused]] const JPH::BodyID& inBodyID, [[maybe_unused]] JPH::uint64 inBodyUserData) override
        {
            Console::printfn("A body went to sleep");
        }
    };

    namespace 
    {
        constexpr JPH::uint cMaxBodies = 1024;
        constexpr JPH::uint cNumBodyMutexes = 0;
        constexpr JPH::uint cMaxBodyPairs = 1024;
        constexpr JPH::uint cMaxContactConstraints = 1024;
        BPLayerInterfaceImpl broad_phase_layer_interface;
        ObjectVsBroadPhaseLayerFilterImpl object_vs_broadphase_layer_filter;
        ObjectLayerPairFilterImpl object_vs_object_layer_filter;
        MyBodyActivationListener body_activation_listener;
        MyContactListener contact_listener;

    }

    PhysicsJolt::PhysicsJolt( PlatformContext& context )
        : PhysicsAPIWrapper(context)
    {
    }

    ErrorCode PhysicsJolt::initPhysicsAPI( [[maybe_unused]] const U8 targetFrameRate, [[maybe_unused]] const F32 simSpeed )
    {
        JPH::Trace = TraceImpl;
        JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl);

        JPH::RegisterDefaultAllocator();

        // Create a factory, this class is responsible for creating instances of classes based on their name or hash and is mainly used for deserialization of saved data.
        // It is not directly used in this example but still required.
        _factory = std::make_unique<JPH::Factory>();
        JPH::Factory::sInstance = _factory.get();

        JPH::RegisterTypes();

        _allocator = std::make_unique<JPH::TempAllocatorImpl>(Bytes::Mega(10u));

        _threadPool = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);

        _physicsSystem = std::make_unique<JPH::PhysicsSystem>();

        _physicsSystem->Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints, broad_phase_layer_interface, object_vs_broadphase_layer_filter, object_vs_object_layer_filter);

        _physicsSystem->SetBodyActivationListener(&body_activation_listener);

        _physicsSystem->SetContactListener(&contact_listener);

        return ErrorCode::NO_ERR;
    }

    bool PhysicsJolt::closePhysicsAPI()
    {
        _physicsSystem.reset();
        _threadPool.reset();
        _allocator.reset();
        JPH::UnregisterTypes();
        _factory.reset();
        JPH::Factory::sInstance = nullptr;

        return true;
    }

    /// Process results
    void PhysicsJolt::frameStartedInternal(const U64 deltaTimeGameUS )
    {
        // If you take larger steps than 1 / 60th of a second you need to do multiple collision steps in order to keep the simulation stable. Do 1 collision step per 1 / 60th of a second (round up).
        const I32 cCollisionSteps = to_I32(CEIL(std::max(60.f / _simulationFrameRate, 1.f)));

        _physicsSystem->OptimizeBroadPhase();
        _physicsSystem->Update(Time::MicrosecondsToSeconds<F32>(deltaTimeGameUS), cCollisionSteps, _allocator.get(), _threadPool.get());
    }

    /// Update actors
    void PhysicsJolt::frameEndedInternal([[maybe_unused]] const U64 deltaTimeGameUS )
    {
    }

    void PhysicsJolt::idle()
    {
    }

    bool PhysicsJolt::initPhysicsScene([[maybe_unused]] Scene& scene )
    {
       return true;
    }

    bool PhysicsJolt::destroyPhysicsScene([[maybe_unused]] const Scene& scene )
    {
        return true;
    }

    PhysicsAsset* PhysicsJolt::createRigidActor([[maybe_unused]] SceneGraphNode* node, [[maybe_unused]] RigidBodyComponent& parentComp )
    {
        return nullptr;
    }

    bool PhysicsJolt::convertActor([[maybe_unused]] PhysicsAsset* actor, [[maybe_unused]] const PhysicsGroup newGroup )
    {
        return true;
    }

    bool PhysicsJolt::intersect([[maybe_unused]] const Ray& intersectionRay, [[maybe_unused]] const float2 range, [[maybe_unused]] vector<SGNRayResult>& intersectionsOut ) const
    {
        return false;
    }

};
