#include "stdafx.h"

#include "config.h"

#include "Headers/RenderPass.h"
#include "Headers/NodeBufferedData.h"

#include "Core/Headers/Kernel.h"
#include "Editor/Headers/Editor.h"
#include "Core/Headers/Configuration.h"
#include "Graphs/Headers/SceneGraph.h"
#include "Managers/Headers/RenderPassManager.h"
#include "Managers/Headers/SceneManager.h"

#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/Headers/CommandBuffer.h"

#include "Rendering/Lighting/Headers/LightPool.h"
#include "Rendering/Camera/Headers/Camera.h"
#include "Geometry/Material/Headers/Material.h"

#include "Scenes/Headers/Scene.h"
#include "Scenes/Headers/SceneEnvironmentProbePool.h"

#include "ECS/Components/Headers/EnvironmentProbeComponent.h"
#include "ECS/Components/Headers/RenderingComponent.h"

namespace Divide
{

    namespace
    {
        // We need a proper, time-based system, to check reflection budget
        namespace ReflectionUtil
        {
            U16 g_reflectionBudget = 0;

            [[nodiscard]] bool isInBudget() noexcept
            {
                return g_reflectionBudget < Config::MAX_REFLECTIVE_NODES_IN_VIEW;
            }
            void resetBudget() noexcept
            {
                g_reflectionBudget = 0;
            }
            void updateBudget() noexcept
            {
                ++g_reflectionBudget;
            }
            [[nodiscard]] U16  currentEntry() noexcept
            {
                return g_reflectionBudget;
            }
        }

        namespace RefractionUtil
        {
            U16 g_refractionBudget = 0;

            [[nodiscard]] bool isInBudget() noexcept
            {
                return g_refractionBudget < Config::MAX_REFRACTIVE_NODES_IN_VIEW;
            }
            void resetBudget() noexcept
            {
                g_refractionBudget = 0;
            }
            void updateBudget() noexcept
            {
                ++g_refractionBudget;
            }
            [[nodiscard]] U16  currentEntry() noexcept
            {
                return g_refractionBudget;
            }
        }
    }

    RenderPass::RenderPass( RenderPassManager& parent, GFXDevice& context, const RenderStage renderStage, const vector<RenderStage>& dependencies )
        : _context( context ),
        _parent( parent ),
        _config( context.context().config() ),
        _stageFlag( renderStage ),
        _dependencies( dependencies ),
        _name( TypeUtil::RenderStageToString( renderStage ) )
    {
        for ( U8 i = 0u; i < to_base( _stageFlag ); ++i )
        {
            const U8 passCountToSkip = TotalPassCountForStage( static_cast<RenderStage>(i) );
            _transformIndexOffset += passCountToSkip * Config::MAX_VISIBLE_NODES;
        }
    }

    RenderPass::BufferData RenderPass::getBufferData( const RenderStagePass stagePass ) const noexcept
    {
        assert( _stageFlag == stagePass._stage );

        BufferData ret{};
        ret._lastCommandCount = &_lastCmdCount;
        ret._lastNodeCount = &_lastNodeCount;
        return ret;
    }

    void RenderPass::render( const PlayerIndex idx, [[maybe_unused]] const Task& parentTask, const SceneRenderState& renderState, GFX::CommandBuffer& bufferInOut, GFX::MemoryBarrierCommand& memCmdInOut ) const
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Scene );

        switch ( _stageFlag )
        {
            case RenderStage::DISPLAY:
            {
                PROFILE_SCOPE( "RenderPass - Main", Profiler::Category::Scene );

                RTDrawDescriptor prePassPolicy = {};
                DisableAll( prePassPolicy._drawMask );
                SetEnabled( prePassPolicy._drawMask, RTAttachmentType::DEPTH, RTColourAttachmentSlot::SLOT_0, true );
                SetEnabled( prePassPolicy._drawMask, RTAttachmentType::COLOUR, GFXDevice::ScreenTargets::VELOCITY, true );
                SetEnabled( prePassPolicy._drawMask, RTAttachmentType::COLOUR, GFXDevice::ScreenTargets::NORMALS, true );
                //prePassPolicy._alphaToCoverage = true;

                RTDrawDescriptor mainPassPolicy = {};
                SetEnabled( mainPassPolicy._drawMask, RTAttachmentType::DEPTH, RTColourAttachmentSlot::SLOT_0, false );
                SetEnabled( mainPassPolicy._drawMask, RTAttachmentType::COLOUR, GFXDevice::ScreenTargets::VELOCITY, false );
                SetEnabled( mainPassPolicy._drawMask, RTAttachmentType::COLOUR, GFXDevice::ScreenTargets::NORMALS, false );

                const RTDrawDescriptor oitCompositionPassPolicy = mainPassPolicy;

                RenderPassParams params{};
                params._singleNodeRenderGUID = -1;
                params._passName = "MainRenderPass";
                params._stagePass = RenderStagePass{ _stageFlag, RenderPassType::COUNT };
                params._targetDescriptorPrePass = prePassPolicy;
                params._targetDescriptorMainPass = mainPassPolicy;
                params._targetDescriptorComposition = oitCompositionPassPolicy;
                params._targetHIZ = RenderTargetNames::HI_Z;
                params._clearDescriptorMainPass._clearDepth = false;
                //params._clearDescriptorMainPass._clearColourDescriptors[0] = { DefaultColours::DIVIDE_BLUE, GFXDevice::ScreenTargets::ALBEDO };
                //Not everything gets drawn during the depth PrePass (E.g. sky)
                params._clearDescriptorPrePass._clearDepth = true;
                params._clearDescriptorPrePass._clearColourDescriptors[1] = { VECTOR4_ZERO, GFXDevice::ScreenTargets::VELOCITY };
                params._clearDescriptorPrePass._clearColourDescriptors[2] = { VECTOR4_ZERO, GFXDevice::ScreenTargets::NORMALS };

                if ( _config.rendering.MSAASamples > 0u )
                {
                    params._targetOIT = RenderTargetNames::OIT_MS;
                    params._target = RenderTargetNames::SCREEN_MS;
                }
                else
                {
                    params._targetOIT = RenderTargetNames::OIT;
                    params._target = RenderTargetNames::SCREEN;
                }

                GFX::EnqueueCommand( bufferInOut, GFX::BeginDebugScopeCommand{ "Main Display Pass" } );
                GFX::EnqueueCommand<GFX::SetClippingStateCommand>( bufferInOut )->_negativeOneToOneDepth = false;

                Camera* playerCamera = Attorney::SceneManagerCameraAccessor::playerCamera( _parent.parent().sceneManager() );
                _parent.doCustomPass( playerCamera, params, bufferInOut, memCmdInOut );

                GFX::EnqueueCommand<GFX::SetClippingStateCommand>( bufferInOut )->_negativeOneToOneDepth = true;
                GFX::EnqueueCommand<GFX::EndDebugScopeCommand>( bufferInOut );
            } break;
            case RenderStage::NODE_PREVIEW:
            {
                if_constexpr( Config::Build::ENABLE_EDITOR )
                {
                    PROFILE_SCOPE( "RenderPass - Node Preview", Profiler::Category::Scene );
                    const Editor& editor = _context.context().editor();
                    if (editor.running() && editor.nodePreviewWindowVisible())
                    {
                        RenderPassParams params = {};
                        params._singleNodeRenderGUID = renderState.singleNodeRenderGUID();
                        params._minExtents.set( 1.0f );
                        params._stagePass = { _stageFlag, RenderPassType::COUNT };
                        params._target = editor.getNodePreviewTarget()._targetID;
                        params._passName = "Node Preview";
                        params._clearDescriptorPrePass._clearDepth = true;
                        params._clearDescriptorMainPass._clearColourDescriptors[0] = { editor.nodePreviewBGColour(), RTColourAttachmentSlot::SLOT_0 };

                        _parent.doCustomPass( editor.nodePreviewCamera(), params, bufferInOut, memCmdInOut );
                    }
                }
            } break;
            case RenderStage::SHADOW:
            {
                PROFILE_SCOPE( "RenderPass - Shadow", Profiler::Category::Scene );
                if ( _config.rendering.shadowMapping.enabled )
                {
                    SceneManager* mgr = _parent.parent().sceneManager();
                    LightPool& lightPool = Attorney::SceneManagerRenderPass::lightPool( mgr );

                    const Camera* camera = Attorney::SceneManagerCameraAccessor::playerCamera( mgr );

                    GFX::EnqueueCommand( bufferInOut, GFX::BeginDebugScopeCommand{ "Shadow Render Stage" } );
                    lightPool.sortLightData( RenderStage::SHADOW, camera->snapshot() );
                    lightPool.generateShadowMaps( *camera, bufferInOut, memCmdInOut );

                    GFX::EnqueueCommand<GFX::EndDebugScopeCommand>( bufferInOut );
                }
            } break;

            case RenderStage::REFLECTION:
            {
                SceneManager* mgr = _parent.parent().sceneManager();
                Camera* camera = Attorney::SceneManagerCameraAccessor::playerCamera( mgr );

                GFX::EnqueueCommand( bufferInOut, GFX::BeginDebugScopeCommand{ "Reflection Pass" } );
                GFX::EnqueueCommand<GFX::SetClippingStateCommand>( bufferInOut )->_negativeOneToOneDepth = false;
                {
                    PROFILE_SCOPE( "RenderPass - Probes", Profiler::Category::Scene );
                    SceneEnvironmentProbePool::Prepare( bufferInOut );

                    SceneEnvironmentProbePool* envProbPool = Attorney::SceneRenderPass::getEnvProbes( mgr->getActiveScene() );
                    envProbPool->lockProbeList();
                    const EnvironmentProbeList& probes = envProbPool->sortAndGetLocked( camera->snapshot()._eye );
                    U32 probeIdx = 0u;
                    for ( const auto& probe : probes )
                    {
                        if ( probe->refresh( bufferInOut, memCmdInOut ) && ++probeIdx == Config::MAX_REFLECTIVE_PROBES_PER_PASS )
                        {
                            break;
                        }
                    }
                    envProbPool->unlockProbeList();
                }
                {
                    PROFILE_SCOPE( "RenderPass - Reflection", Profiler::Category::Scene );
                    static VisibleNodeList s_Nodes;
                    //Update classic reflectors (e.g. mirrors, water, etc)
                    //Get list of reflective nodes from the scene manager
                    mgr->getSortedReflectiveNodes( camera, RenderStage::REFLECTION, true, s_Nodes );

                    // While in budget, update reflections
                    ReflectionUtil::resetBudget();
                    for ( size_t i = 0; i < s_Nodes.size(); ++i )
                    {
                        const VisibleNode& node = s_Nodes.node( i );
                        RenderingComponent* const rComp = node._node->get<RenderingComponent>();
                        if ( Attorney::RenderingCompRenderPass::updateReflection( *rComp,
                                                                                  ReflectionUtil::currentEntry(),
                                                                                  ReflectionUtil::isInBudget(),
                                                                                  camera,
                                                                                  renderState,
                                                                                  bufferInOut,
                                                                                  memCmdInOut ) )
                        {
                            ReflectionUtil::updateBudget();
                        }
                    }
                }
                GFX::EnqueueCommand<GFX::SetClippingStateCommand>( bufferInOut )->_negativeOneToOneDepth = true;
                GFX::EnqueueCommand<GFX::EndDebugScopeCommand>( bufferInOut );

            } break;

            case RenderStage::REFRACTION:
            {
                static VisibleNodeList s_Nodes;

                GFX::EnqueueCommand( bufferInOut, GFX::BeginDebugScopeCommand{ "Refraction Pass" } );
                GFX::EnqueueCommand<GFX::SetClippingStateCommand>( bufferInOut )->_negativeOneToOneDepth = false;

                PROFILE_SCOPE( "RenderPass - Refraction", Profiler::Category::Scene );
                // Get list of refractive nodes from the scene manager
                const SceneManager* mgr = _parent.parent().sceneManager();
                Camera* camera = Attorney::SceneManagerCameraAccessor::playerCamera( mgr );
                {
                    mgr->getSortedRefractiveNodes( camera, RenderStage::REFRACTION, true, s_Nodes );
                    // While in budget, update refractions
                    RefractionUtil::resetBudget();
                    for ( size_t i = 0; i < s_Nodes.size(); ++i )
                    {
                        const VisibleNode& node = s_Nodes.node( i );
                        RenderingComponent* const rComp = node._node->get<RenderingComponent>();
                        if ( Attorney::RenderingCompRenderPass::updateRefraction( *rComp,
                                                                                  RefractionUtil::currentEntry(),
                                                                                  RefractionUtil::isInBudget(),
                                                                                  camera,
                                                                                  renderState,
                                                                                  bufferInOut,
                                                                                  memCmdInOut ) )
                        {
                            RefractionUtil::updateBudget();
                        }
                    }
                }

                GFX::EnqueueCommand<GFX::SetClippingStateCommand>( bufferInOut )->_negativeOneToOneDepth = true;
                GFX::EnqueueCommand<GFX::EndDebugScopeCommand>( bufferInOut );

            } break;

            case RenderStage::COUNT:
                DIVIDE_UNEXPECTED_CALL();
                break;
        };
    }

};