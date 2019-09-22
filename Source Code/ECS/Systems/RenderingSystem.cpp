#include "stdafx.h"

#include "Headers/RenderingSystem.h"

namespace Divide {
    RenderingSystem::RenderingSystem(ECS::ECSEngine& parentEngine, PlatformContext& context)
        : ECSSystem(parentEngine),
          PlatformContextComponent(context)
    {

    }

    RenderingSystem::~RenderingSystem()
    {

    }

    void RenderingSystem::PreUpdate(F32 dt) {
        U64 microSec = Time::MillisecondsToMicroseconds(dt);

        auto rComp = _container->begin();
        auto rCompEnd = _container->end();
        for (;rComp != rCompEnd; ++rComp)
        {
            rComp->PreUpdate(microSec);
        }
    }

    void RenderingSystem::Update(F32 dt) {
        U64 microSec = Time::MillisecondsToMicroseconds(dt);

        auto rComp = _container->begin();
        auto rCompEnd = _container->end();
        for (; rComp != rCompEnd; ++rComp)
        {
            rComp->Update(microSec);
        }
    }

    void RenderingSystem::PostUpdate(F32 dt) {
        U64 microSec = Time::MillisecondsToMicroseconds(dt);

        auto rComp = _container->begin();
        auto rCompEnd = _container->end();
        for (; rComp != rCompEnd; ++rComp)
        {
            rComp->PostUpdate(microSec);
        }
    }

    void RenderingSystem::FrameEnded() {

        auto comp = _container->begin();
        auto compEnd = _container->end();
        for (; comp != compEnd; ++comp) {
            comp->FrameEnded();
        }
    }

    bool RenderingSystem::saveCache(const SceneGraphNode& sgn, ByteBuffer& outputBuffer) {
        RenderingComponent* rComp = sgn.GetComponent<RenderingComponent>();
        if (rComp != nullptr && !rComp->saveCache(outputBuffer)) {
            return false;
        }

        return Super::saveCache(sgn, outputBuffer);
    }

    bool RenderingSystem::loadCache(SceneGraphNode& sgn, ByteBuffer& inputBuffer) {
        RenderingComponent* rComp = sgn.GetComponent<RenderingComponent>();
        if (rComp != nullptr && !rComp->loadCache(inputBuffer)) {
            return false;
        }

        return Super::loadCache(sgn, inputBuffer);
    }
}; //namespace Divide