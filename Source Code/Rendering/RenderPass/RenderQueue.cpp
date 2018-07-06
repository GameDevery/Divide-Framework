#include "stdafx.h"

#include "Headers/RenderQueue.h"

#include "Core/Headers/Kernel.h"
#include "Core/Headers/Console.h"
#include "Core/Headers/TaskPool.h"
#include "Core/Headers/Application.h"
#include "Utility/Headers/Localization.h"
#include "Graphs/Headers/SceneGraphNode.h"
#include "Geometry/Shapes/Headers/Object3D.h"
#include "Geometry/Material/Headers/Material.h"

namespace Divide {

RenderQueue::RenderQueue(GFXDevice& context)
    : _context(context)
{
    _renderBins.fill(nullptr);
    _activeBins.reserve(to_base(RenderBinType::COUNT));
}

RenderQueue::~RenderQueue()
{
    for (RenderBin* bin : _renderBins) {
        MemoryManager::DELETE(bin);
    }
    _renderBins.fill(nullptr);
    _activeBins.clear();
}

U16 RenderQueue::getRenderQueueStackSize() const {
    U16 temp = 0;
    for (RenderBin* bin : _activeBins) {
        temp += bin->getBinSize();
    }
    return temp;
}

RenderingOrder::List RenderQueue::getSortOrder(RenderBinType rbType) {
    RenderingOrder::List sortOrder = RenderingOrder::List::COUNT;
    switch (rbType) {
        case RenderBinType::RBT_TRANSPARENT:
        case RenderBinType::RBT_OPAQUE: {
            // Opaque items should be rendered front to back in depth passes for early-Z reasons
            sortOrder = !_context.isDepthStage() ? RenderingOrder::List::BY_STATE
                                                 : RenderingOrder::List::FRONT_TO_BACK;
        } break;
        case RenderBinType::RBT_SKY: {
            sortOrder = RenderingOrder::List::NONE;
        } break;
        case RenderBinType::RBT_IMPOSTOR:
        case RenderBinType::RBT_TERRAIN:
        case RenderBinType::RBT_DECAL: {
            // No need to sort decals in depth passes as they're small on screen and processed post-opaque pass
            sortOrder = !_context.isDepthStage() ? RenderingOrder::List::FRONT_TO_BACK
                                                 : RenderingOrder::List::NONE;
        } break;
        case RenderBinType::RBT_TRANSLUCENT: {
            // Translucent items should be rendered by material in depth passes to avoid useless material switches
            // Small cost for bypassing early-Z checks, but translucent items should be in the minority on the screen anyway
            // and the Opaque pass should have finished by now
            sortOrder = !_context.isDepthStage() ? RenderingOrder::List::BACK_TO_FRONT
                                                 : RenderingOrder::List::BY_STATE;
        } break;
        default: {
            Console::errorfn(Locale::get(_ID("ERROR_INVALID_RENDER_BIN_CREATION")));
        } break;
    };
    
    return sortOrder;
}

RenderBin* RenderQueue::getOrCreateBin(RenderBinType rbType) {
    RenderBin* temp = getBin(rbType);
    if (temp != nullptr) {
        return temp;
    }

    temp = MemoryManager_NEW RenderBin(_context, rbType);
    // Bins are sorted by their type
    _renderBins[rbType._to_integral()] = temp;
    
    _activeBins.resize(0);
    std::back_insert_iterator<vector<RenderBin*>> back_it(_activeBins);
    auto const usedPredicate = [](RenderBin* ptr) { return ptr != nullptr; };
    std::copy_if(std::begin(_renderBins), std::end(_renderBins), back_it, usedPredicate);

    return temp;
}

RenderBin* RenderQueue::getBinForNode(const std::shared_ptr<SceneNode>& node,
                                      const Material_ptr& matInstance) {
    assert(node != nullptr);
    switch (node->getType()) {
        case SceneNodeType::TYPE_LIGHT: return getOrCreateBin(RenderBinType::RBT_IMPOSTOR);
        case SceneNodeType::TYPE_PARTICLE_EMITTER: return getOrCreateBin(RenderBinType::RBT_TRANSLUCENT);
        case SceneNodeType::TYPE_VEGETATION_GRASS: return getOrCreateBin(RenderBinType::RBT_TRANSPARENT);
        case SceneNodeType::TYPE_SKY: return getOrCreateBin(RenderBinType::RBT_SKY);

        // Water is also opaque as refraction and reflection are separate textures
        case SceneNodeType::TYPE_WATER:
        case SceneNodeType::TYPE_OBJECT3D: {
        case SceneNodeType::TYPE_VEGETATION_TREES:
            if (node->getType() == SceneNodeType::TYPE_OBJECT3D) {
                Object3D::ObjectType type = static_cast<Object3D*>(node.get())->getObjectType();
                switch (type) {
                    case Object3D::ObjectType::TERRAIN: return getOrCreateBin(RenderBinType::RBT_TERRAIN);
                    case Object3D::ObjectType::DECAL: return getOrCreateBin(RenderBinType::RBT_DECAL);
                }
            }
            // Check if the object has a material with transparency/translucency
            if (matInstance) {
                // Add it to the appropriate bin if so ...
                if (matInstance->hasTransparency()) {
                    if (matInstance->hasTranslucency()) {
                        return getOrCreateBin(RenderBinType::RBT_TRANSLUCENT);
                    }
                    return getOrCreateBin(RenderBinType::RBT_TRANSPARENT);
                }
            }
            //... else add it to the general geometry bin
            return getOrCreateBin(RenderBinType::RBT_OPAQUE);
        }
    }
    return nullptr;
}

void RenderQueue::addNodeToQueue(const SceneGraphNode& sgn, const RenderStagePass& stage, const vec3<F32>& eyePos) {
    static Material_ptr defaultMat;

    RenderingComponent* const renderingCmp = sgn.get<RenderingComponent>();
    RenderBin* rb = getBinForNode(sgn.getNode(),
                                  renderingCmp
                                    ? renderingCmp->getMaterialInstance()
                                    : defaultMat);
    if (rb) {
        rb->addNodeToBin(sgn, stage, eyePos);
    }
}

void RenderQueue::populateRenderQueues(const RenderStagePass& renderStagePass) {
    for (RenderBin* renderBin : _activeBins) {
        if (!renderBin->empty()) {
            renderBin->populateRenderQueue(renderStagePass);
        }
    }
}

void RenderQueue::postRender(const SceneRenderState& renderState, const RenderStagePass& renderStagePass, GFX::CommandBuffer& bufferInOut) {
    for (RenderBin* renderBin : _activeBins) {
        renderBin->postRender(renderState, renderStagePass, bufferInOut);
    }
}

void RenderQueue::sort() {
    // How many elements should a renderbin contain before we decide that sorting 
    // should happen on a separate thread
    static const U16 threadBias = 16;

    TaskPool& pool = _context.parent().taskPool();
    TaskHandle sortTask = CreateTask(pool, DELEGATE_CBK<void, const Task&>());
    for (RenderBin* renderBin : _activeBins) {
        if (!renderBin->empty()) {
            RenderingOrder::List sortOrder = getSortOrder(renderBin->getType());

            if (renderBin->getBinSize() > threadBias) {
                Task* child = sortTask.addChildTask(CreateTask(pool,
                                                               [renderBin, sortOrder](const Task& parentTask) {
                    renderBin->sort(sortOrder, parentTask);
                }));

                child->startTask(Task::TaskPriority::HIGH);
            } else {
                renderBin->sort(sortOrder);
            }
        }
    }
    sortTask.startTask(Task::TaskPriority::MAX).wait();
}

void RenderQueue::refresh() {
    for (RenderBin* renderBin : _activeBins) {
        renderBin->refresh();
    }
}

const RenderQueue::SortedQueues& RenderQueue::getSortedQueues() {
    for (RenderBin* renderBin : _activeBins) {
        vector<SceneGraphNode*>& nodes = _sortedQueues[renderBin->getType()._to_integral()];
        renderBin->getSortedNodes(nodes);
    }

    return _sortedQueues;
}

};