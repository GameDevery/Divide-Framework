#include "stdafx.h"

#include "Headers/RenderingSystem.h"

#include "Graphs/Headers/SceneNode.h"
#include "Geometry/Material/Headers/Material.h"

namespace Divide
{
    RenderingSystem::RenderingSystem( ECS::ECSEngine& parentEngine, PlatformContext& context )
        : PlatformContextComponent( context )
        , ECSSystem( parentEngine )
    {
    }

    RenderingSystem::~RenderingSystem()
    {
    }

    void RenderingSystem::PreUpdate( const F32 dt )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Scene );

        Parent::PreUpdate( dt );

        const U64 microSec = Time::MillisecondsToMicroseconds( dt );

        for ( RenderingComponent* comp : _componentCache )
        {
            if ( comp->_materialInstance != nullptr )
            {
                comp->_materialUpdateMask |= comp->_materialInstance->update( microSec );
            }
        }

        for ( RenderingComponent* comp : _componentCache )
        {
            comp->updateReflectRefractDescriptors( false, false );
            if ( comp->rebuildDrawCommands() || comp->parentSGN()->getNode().rebuildDrawCommands() )
            {
                comp->parentSGN()->getNode().rebuildDrawCommands( false );
                comp->rebuildDrawCommands( false );
                SetBit( comp->_materialUpdateMask, MaterialUpdateResult::NEW_SHADER );
                SetBit( comp->_materialUpdateMask, MaterialUpdateResult::NEW_CULL );
            }
        }
    }

    void RenderingSystem::Update( const F32 dt )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Scene );

        Parent::Update( dt );

        const U64 microSec = Time::MillisecondsToMicroseconds( dt );

        for ( RenderingComponent* comp : _componentCache )
        {
            if ( comp->_materialUpdateMask == to_base( MaterialUpdateResult::OK ) )
            {
                continue;
            }
            DIVIDE_ASSERT( comp->_materialInstance != nullptr );

            bool packagesCleared = false;
            if ( TestBit( comp->_materialUpdateMask, MaterialUpdateResult::NEW_SHADER ) ||
                 TestBit( comp->_materialUpdateMask, MaterialUpdateResult::NEW_CULL ) )
            {
                comp->clearDrawPackages();
                comp->_materialInstance->clearRenderStates();
                packagesCleared = true;
            }

            if ( TestBit( comp->_materialUpdateMask, MaterialUpdateResult::NEW_CULL ) )
            {
                comp->_materialInstance->updateCullState();
            }
            if ( TestBit( comp->_materialUpdateMask, MaterialUpdateResult::NEW_TRANSPARENCY ) )
            {
                NOP();
            }

            comp->updateReflectRefractDescriptors( TestBit( comp->_materialUpdateMask, MaterialUpdateResult::NEW_REFLECTION ),
                                                   TestBit( comp->_materialUpdateMask, MaterialUpdateResult::NEW_REFRACTION ) );
        }

        Material::Update( microSec );
    }

    void RenderingSystem::PostUpdate( const F32 dt )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Scene );

        Parent::PostUpdate( dt );

        for ( RenderingComponent* comp : _componentCache )
        {
            comp->_materialUpdateMask = to_base( MaterialUpdateResult::OK );
        }
    }

    void RenderingSystem::OnFrameStart()
    {
        Parent::OnFrameStart();
    }

    void RenderingSystem::OnFrameEnd()
    {
        Parent::OnFrameEnd();
    }

    bool RenderingSystem::saveCache( const SceneGraphNode* sgn, ByteBuffer& outputBuffer )
    {
        if ( Parent::saveCache( sgn, outputBuffer ) )
        {
            const RenderingComponent* rComp = sgn->GetComponent<RenderingComponent>();
            return (rComp == nullptr || rComp->saveCache( outputBuffer ));
        }

        return false;
    }

    bool RenderingSystem::loadCache( SceneGraphNode* sgn, ByteBuffer& inputBuffer )
    {
        if ( Parent::loadCache( sgn, inputBuffer ) )
        {
            RenderingComponent* rComp = sgn->GetComponent<RenderingComponent>();
            return (rComp == nullptr || rComp->loadCache( inputBuffer ));
        }

        return false;
    }
} //namespace Divide