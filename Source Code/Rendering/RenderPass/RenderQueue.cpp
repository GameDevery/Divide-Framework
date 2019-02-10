#include "stdafx.h"

#include "Headers/RenderQueue.h"

#include "Core/Headers/Kernel.h"
#include "Core/Headers/Console.h"
#include "Core/Headers/TaskPool.h"
#include "Core/Headers/Application.h"
#include "Core/Headers/PlatformContext.h"
#include "Utility/Headers/Localization.h"
#include "Graphs/Headers/SceneGraphNode.h"
#include "Geometry/Shapes/Headers/Object3D.h"
#include "Geometry/Material/Headers/Material.h"
#include "Managers/Headers/RenderPassManager.h"
#include "ECS/Components/Headers/RenderingComponent.h"

namespace Divide {

RenderQueue::RenderQueue(Kernel& parent)
    : KernelComponent(parent)
{
    for (RenderBinType rbType : RenderBinType::_values()) {
        if (rbType._value == RenderBinType::RBT_COUNT) {
            continue;
        }

        _renderBins[rbType._value] = MemoryManager_NEW RenderBin(rbType);
    }
}

RenderQueue::~RenderQueue()
{
    for (RenderBin* bin : _renderBins) {
        MemoryManager::DELETE(bin);
    }

    _renderBins.fill(nullptr);
}

U16 RenderQueue::getRenderQueueStackSize(RenderStage stage) const {
    U16 temp = 0;
    for (RenderBin* bin : _renderBins) {
        if (bin != nullptr) {
            temp += bin->getBinSize(stage);
        }
    }
    return temp;
}

RenderingOrder::List RenderQueue::getSortOrder(RenderStagePass stagePass, RenderBinType rbType) {
    RenderingOrder::List sortOrder = RenderingOrder::List::COUNT;
    switch (rbType) {
        case RenderBinType::RBT_OPAQUE: {
            // Opaque items should be rendered front to back in depth passes for early-Z reasons
            sortOrder = !stagePass.isDepthPass() ? RenderingOrder::List::BY_STATE
                                                 : RenderingOrder::List::FRONT_TO_BACK;
        } break;
        case RenderBinType::RBT_SKY: {
            sortOrder = RenderingOrder::List::NONE;
        } break;
        case RenderBinType::RBT_IMPOSTOR:
        case RenderBinType::RBT_TERRAIN: {
            // No need to sort decals in depth passes as they're small on screen and processed post-opaque pass
            sortOrder = !stagePass.isDepthPass() ? RenderingOrder::List::FRONT_TO_BACK
                                                 : RenderingOrder::List::NONE;
        } break;
        case RenderBinType::RBT_TRANSLUCENT: {
             // We are using weighted blended OIT. State is fine (and faster)
            sortOrder = RenderingOrder::List::BY_STATE;
        } break;
        default: {
            Console::errorfn(Locale::get(_ID("ERROR_INVALID_RENDER_BIN_CREATION")));
        } break;
    };
    
    return sortOrder;
}

RenderBin* RenderQueue::getBinForNode(const SceneGraphNode& node, const Material_ptr& matInstance) {
    switch (node.getNode().type()) {
        case SceneNodeType::TYPE_EMPTY:
        {
            if (BitCompare(node.componentMask(), ComponentType::SPOT_LIGHT) ||
                BitCompare(node.componentMask(), ComponentType::POINT_LIGHT) ||
                BitCompare(node.componentMask(), ComponentType::DIRECTIONAL_LIGHT))
            {
                return _renderBins[RenderBinType::RBT_IMPOSTOR];
            }
            /*if (BitCompare(node.componentMask(), ComponentType::PARTICLE_EMITTER_COMPONENT) ||
                BitCompare(node.componentMask(), ComponentType::GRASS_COMPONENT))
            {
                return _renderBins[RenderBinType::RBT_TRANSLUCENT];
            }*/
            return nullptr;
        }

        case SceneNodeType::TYPE_VEGETATION:
        case SceneNodeType::TYPE_PARTICLE_EMITTER:
            return _renderBins[RenderBinType::RBT_TRANSLUCENT];

        case SceneNodeType::TYPE_SKY:
            return _renderBins[RenderBinType::RBT_SKY];

        case SceneNodeType::TYPE_WATER:
        case SceneNodeType::TYPE_INFINITEPLANE:
            return _renderBins[RenderBinType::RBT_TERRAIN];

        // Water is also opaque as refraction and reflection are separate textures
        // We may want to break this stuff up into mesh rendering components and not care about specifics anymore (i.e. just material checks)
        //case SceneNodeType::TYPE_WATER:
        case SceneNodeType::TYPE_OBJECT3D: {
            if (node.getNode().type() == SceneNodeType::TYPE_OBJECT3D) {
                ObjectType type = node.getNode<Object3D>().getObjectType();
                switch (type) {
                    case ObjectType::TERRAIN:
                        return _renderBins[RenderBinType::RBT_TERRAIN];

                    case ObjectType::DECAL:
                        return _renderBins[RenderBinType::RBT_TRANSLUCENT];
                }
            }
            // Check if the object has a material with transparency/translucency
            if (matInstance != nullptr && matInstance->hasTransparency()) {
                // Add it to the appropriate bin if so ...
                return _renderBins[RenderBinType::RBT_TRANSLUCENT];
            }

            //... else add it to the general geometry bin
            return _renderBins[RenderBinType::RBT_OPAQUE];
        }
    }
    return nullptr;
}

void RenderQueue::addNodeToQueue(const SceneGraphNode& sgn, RenderStagePass stage, F32 minDistToCameraSq) {
    RenderingComponent* const renderingCmp = sgn.get<RenderingComponent>();
    // We need a rendering component to render the node
    if (renderingCmp != nullptr) {
        RenderBin* rb = getBinForNode(sgn, renderingCmp->getMaterialInstance());
        assert(rb != nullptr);

        rb->addNodeToBin(sgn, stage, minDistToCameraSq);
    }
}

void RenderQueue::populateRenderQueues(RenderStagePass stagePass, RenderBinType binType, vectorEASTL<RenderPackage*>& queueInOut) {
    if (binType._value == RenderBinType::RBT_COUNT) {
        for (RenderBin* renderBin : _renderBins) {
            renderBin->populateRenderQueue(stagePass, queueInOut);
        }
    } else {
        _renderBins[binType]->populateRenderQueue(stagePass, queueInOut);
    }
}

void RenderQueue::postRender(const SceneRenderState& renderState, RenderStagePass stagePass, GFX::CommandBuffer& bufferInOut) {
    for (RenderBin* renderBin : _renderBins) {
        renderBin->postRender(renderState, stagePass, bufferInOut);
    }
}

void RenderQueue::sort(RenderStagePass stagePass) {
    // How many elements should a renderbin contain before we decide that sorting should happen on a separate thread
    static const U16 threadBias = 16;

    TaskPool& pool = parent().platformContext().taskPool(TaskPoolType::Render);
    TaskHandle sortTask = CreateTask(pool, DELEGATE_CBK<void, const Task&>());

    for (RenderBin* renderBin : _renderBins) {
        if (!renderBin->empty(stagePass._stage)) {
            RenderingOrder::List sortOrder = getSortOrder(stagePass, renderBin->getType());

            if (renderBin->getBinSize(stagePass._stage) > threadBias) {
                CreateTask(pool,
                           &sortTask,
                            [renderBin, sortOrder, stagePass](const Task& parentTask) {
                                renderBin->sort(stagePass._stage, sortOrder, parentTask);
                            }).startTask();
            } else {
                renderBin->sort(stagePass._stage, sortOrder);
            }
        }
    }
    sortTask.startTask().wait();
}

void RenderQueue::refresh(RenderStage stage) {
    for (RenderBin* renderBin : _renderBins) {
        renderBin->refresh(stage);
    }
}

void RenderQueue::getSortedQueues(RenderStagePass stagePass, SortedQueues& queuesOut, U16& countOut) const {
    if (stagePass._passType == RenderPassType::PRE_PASS) {
        constexpr RenderBinType rbTypes[] = {
            RenderBinType::RBT_OPAQUE,
            RenderBinType::RBT_IMPOSTOR,
            RenderBinType::RBT_TERRAIN,
            RenderBinType::RBT_SKY,
            RenderBinType::RBT_TRANSLUCENT
        };
         
        for (RenderBinType type : rbTypes) {
            RenderBin* renderBin = _renderBins[type];
            vectorEASTL<SceneGraphNode*>& nodes = queuesOut[type];
            renderBin->getSortedNodes(stagePass._stage, nodes, countOut);
        }
    } else {
        for (RenderBin* renderBin : _renderBins) {
            vectorEASTL<SceneGraphNode*>& nodes = queuesOut[renderBin->getType()];
            renderBin->getSortedNodes(stagePass._stage, nodes, countOut);
        }
    }
}

};