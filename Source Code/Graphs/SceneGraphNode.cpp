#include "stdafx.h"

#include "Headers/SceneGraph.h"
#include "Headers/SceneGraphNode.h"

#include "Core/Headers/PlatformContext.h"
#include "Scenes/Headers/SceneState.h"
#include "Core/Time/Headers/ApplicationTimer.h"
#include "Managers/Headers/SceneManager.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Geometry/Shapes/Headers/Object3D.h"
#include "Geometry/Material/Headers/Material.h"
#include "Geometry/Shapes/Headers/SkinnedSubMesh.h"
#include "Environment/Water/Headers/Water.h"
#include "Environment/Terrain/Headers/Terrain.h"
#include "Platform/Video/Headers/RenderPackage.h"
#include "Platform/Video/Shaders/Headers/ShaderProgram.h"

#include "Editor/Headers/Editor.h"

#include "ECS/Systems/Headers/ECSManager.h"

#include "ECS/Components/Headers/IKComponent.h"
#include "ECS/Components/Headers/RagdollComponent.h"
#include "ECS/Components/Headers/BoundsComponent.h"
#include "ECS/Components/Headers/AnimationComponent.h"
#include "ECS/Components/Headers/TransformComponent.h"
#include "ECS/Components/Headers/NetworkingComponent.h"
#include "ECS/Components/Headers/SpotLightComponent.h"
#include "ECS/Components/Headers/PointLightComponent.h"
#include "ECS/Components/Headers/DirectionalLightComponent.h"


namespace Divide {

namespace {
    constexpr size_t INITIAL_CHILD_COUNT = 16;

    inline bool PropagateFlagToChildren(SceneGraphNode::Flags flag) {
        return flag == SceneGraphNode::Flags::SELECTED || 
               flag == SceneGraphNode::Flags::HOVERED ||
               flag == SceneGraphNode::Flags::ACTIVE ||
               flag == SceneGraphNode::Flags::VISIBILITY_LOCKED;
    }
};

SceneGraphNode::SceneGraphNode(SceneGraph& sceneGraph, const SceneGraphNodeDescriptor& descriptor)
    : GUIDWrapper(),
      PlatformContextComponent(sceneGraph.parentScene().context()),
      _sceneGraph(sceneGraph),
      _compManager(sceneGraph.GetECSEngine().GetComponentManager()),
      _serialize(descriptor._serialize),
      _node(descriptor._node),
      _instanceCount(to_U32(descriptor._instanceCount)),
      _usageContext(descriptor._usageContext),
      _relationshipCache(*this),
      _children{}
{
    _children.reserve(INITIAL_CHILD_COUNT);
    std::atomic_init(&_childCount, 0u);

    _name = (descriptor._name.empty() ? Util::StringFormat("%s_SGN", (_node->resourceName().empty() ? "ERROR"   
                                                                                                    : _node->resourceName().c_str())).c_str()
                                      : descriptor._name);

    setFlag(Flags::ACTIVE);
    clearFlag(Flags::VISIBILITY_LOCKED);

    if (_node == nullptr) {
        _node = std::make_shared<SceneNode>(sceneGraph.parentScene().resourceCache(),
                                              GUIDWrapper::generateGUID(),
                                              "EMPTY",
                                              "EMPTY",
                                              "",
                                              SceneNodeType::TYPE_EMPTY,
                                              to_base(ComponentType::TRANSFORM));
    }

    if (_node->type() == SceneNodeType::TYPE_EMPTY || _node->type() == SceneNodeType::TYPE_ROOT) {
        _node->load();
    }

    assert(_node != nullptr);

    AddComponents(descriptor._componentMask, false);
    AddComponents(_node->requiredComponentMask(), false);

    Attorney::SceneNodeSceneGraph::registerSGNParent(*_node, this);
}

/// If we are destroying the current graph node
SceneGraphNode::~SceneGraphNode()
{
    Console::printfn(Locale::get(_ID("REMOVE_SCENEGRAPH_NODE")), name().c_str(), _node->resourceName().c_str());

    Attorney::SceneGraphSGN::onNodeDestroy(_sceneGraph, *this);
    Attorney::SceneNodeSceneGraph::unregisterSGNParent(*_node, this);
    if (Attorney::SceneNodeSceneGraph::parentCount(*_node) == 0) {
        assert(_node.use_count() <= Attorney::SceneNodeSceneGraph::maxReferenceCount(*_node));

        _node.reset();
    }

    // Bottom up
    for (U32 i = 0; i < getChildCount(); ++i) {
        _sceneGraph.destroySceneGraphNode(_children[i]);
    }
    _children.clear();
    //_childCount.store(0u);

    _compManager->RemoveAllComponents(GetEntityID());
}

ECS::ECSEngine& SceneGraphNode::GetECSEngine() {
    return _sceneGraph.GetECSEngine();
}

void SceneGraphNode::AddComponents(U32 componentMask, bool allowDuplicates) {

    for (ComponentType::_integral i = 1; i < ComponentType::COUNT + 1; ++i) {
        const U16 componentBit = 1 << i;

        // Only add new components;
        if (BitCompare(componentMask, to_U32(componentBit)) && (allowDuplicates || !BitCompare(_componentMask, to_U32(componentBit)))) {
            _componentMask |= componentBit;
            SGNComponent::construct(ComponentType::_from_integral(componentBit), *this);
        }
    };
}

void SceneGraphNode::RemoveComponents(U32 componentMask) {
    for (ComponentType::_integral i = 1; i < ComponentType::COUNT + 1; ++i) {
        const U16 componentBit = 1 << i;
        if (BitCompare(componentMask, to_U32(componentBit)) && BitCompare(_componentMask, to_U32(componentBit))) {
            SGNComponent::destruct(ComponentType::_from_integral(componentBit), *this);
        }
    }
}

void SceneGraphNode::setTransformDirty(U32 transformMask) {
    Attorney::SceneGraphSGN::onNodeTransform(_sceneGraph, *this);

    SharedLock<SharedMutex> r_lock(_childLock);
    for (SceneGraphNode* node : _children) {
        TransformComponent* tComp = node->get<TransformComponent>();
        if (tComp != nullptr) {
            Attorney::TransformComponentSGN::onParentTransformDirty(*tComp, transformMask);
        }
    }
}

void SceneGraphNode::changeUsageContext(const NodeUsageContext& newContext) {
    _usageContext = newContext;

    TransformComponent* tComp = get<TransformComponent>();
    if (tComp) {
        Attorney::TransformComponentSGN::onParentUsageChanged(*tComp, _usageContext);
    }

    RenderingComponent* rComp = get<RenderingComponent>();
    if (rComp) {
        Attorney::RenderingComponentSGN::onParentUsageChanged(*rComp, _usageContext);
    }
}

/// Change current SceneGraphNode's parent
void SceneGraphNode::setParent(SceneGraphNode& parent) {
    assert(parent.getGUID() != getGUID());
    
    { //Clear old parent
        if (_parent) {
            if (*_parent == parent) {
                return;
            }
            // Remove us from the old parent's children map
            _parent->removeChildNode(*this, false);
        }
    }
    // Set the parent pointer to the new parent
    _parent = &parent;

    {// Add ourselves in the new parent's children map
        {
            parent._childCount.fetch_add(1);
            UniqueLock<SharedMutex> w_lock(parent._childLock);
            parent._children.push_back(this);
        }
        Attorney::SceneGraphSGN::onNodeAdd(_sceneGraph, *this);
        // That's it. Parent Transforms will be updated in the next render pass;
        parent.invalidateRelationshipCache();
    }
    {// Carry over new parent's flags and settings
        constexpr Flags flags[] = { Flags::SELECTED, Flags::HOVERED, Flags::ACTIVE, Flags::VISIBILITY_LOCKED };
        for (Flags flag : flags) {
            if (parent.hasFlag(flag)) {
                setFlag(flag);
            } else {
                clearFlag(flag);
            }
        }

        changeUsageContext(parent.usageContext());
    }
}

/// Add a new SceneGraphNode to the current node's child list based on a SceneNode
SceneGraphNode* SceneGraphNode::addChildNode(const SceneGraphNodeDescriptor& descriptor) {
    // Create a new SceneGraphNode with the SceneNode's info
    // We need to name the new SceneGraphNode
    // If we did not supply a custom name use the SceneNode's name

    SceneGraphNode* sceneGraphNode = _sceneGraph.createSceneGraphNode(_sceneGraph, descriptor);
    assert(sceneGraphNode != nullptr && sceneGraphNode->_node->getState() != ResourceState::RES_CREATED);

    // Set the current node as the new node's parent
    sceneGraphNode->setParent(*this);

    if (sceneGraphNode->_node->getState() == ResourceState::RES_LOADED) {
        postLoad(*sceneGraphNode->_node, *sceneGraphNode);
    } else if (sceneGraphNode->_node->getState() == ResourceState::RES_LOADING) {
        setFlag(Flags::LOADING);
        sceneGraphNode->_node->addStateCallback(ResourceState::RES_LOADED,
            [this, sceneGraphNode](CachedResource* res) {
                postLoad(*(static_cast<SceneNode*>(res)), *(sceneGraphNode));
                clearFlag(Flags::LOADING);
            }
        );
    }
    // return the newly created node
    return sceneGraphNode;
}

void SceneGraphNode::postLoad(SceneNode& sceneNode, SceneGraphNode& sgn) {
    Attorney::SceneNodeSceneGraph::postLoad(sceneNode, sgn);
    sgn.Hacks._editorComponents.emplace_back(&Attorney::SceneNodeSceneGraph::getEditorComponent(sceneNode));
}

bool SceneGraphNode::removeNodesByType(SceneNodeType nodeType) {
    // Bottom-up pattern
    U32 removalCount = 0, childRemovalCount = 0;
    forEachChild([nodeType, &childRemovalCount](SceneGraphNode* child, I32 /*childIdx*/) {
        if (child->removeNodesByType(nodeType)) {
            ++childRemovalCount;
        }
        return true;
    });

    {
        SharedLock<SharedMutex> r_lock(_childLock);
        const U32 count = to_U32(_children.size());
        for (U32 i = 0; i < count; ++i) {
            if (_children[i]->getNode().type() == nodeType) {
                _sceneGraph.addToDeleteQueue(this, i);
                ++removalCount;
            }
        }
    }

    if (removalCount > 0) {
        return true;
    }

    return childRemovalCount > 0;
}

bool SceneGraphNode::removeChildNode(const SceneGraphNode& node, bool recursive) {

    I64 targetGUID = node.getGUID();
    {
        SharedLock<SharedMutex> r_lock(_childLock);
        const U32 count = to_U32(_children.size());
        for (U32 i = 0; i < count; ++i) {
            if (_children[i]->getGUID() == targetGUID) {
                _sceneGraph.addToDeleteQueue(this, i);
                return true;
            }
        }
    }

    // If this didn't finish, it means that we found our node
    return !recursive || 
           !forEachChild([&node](SceneGraphNode* child, I32 /*childIdx*/) {
                if (child->removeChildNode(node, true)) {
                    return false;
                }
                return true;
            });
}

void SceneGraphNode::postLoad() {
    SendEvent(
    {
        ECS::CustomEvent::Type::EntityPostLoad
    });
}

bool SceneGraphNode::isChildOfType(U16 typeMask, bool ignoreRoot) const {
    if (ignoreRoot) {
        ClearBit(typeMask, to_base(SceneNodeType::TYPE_ROOT));
    }
    SceneGraphNode* parentNode = parent();
    while (parentNode != nullptr) {
        if (BitCompare(typeMask, to_base(parentNode->getNode<>().type()))) {
            return true;
        }
        parentNode = parentNode->parent();
    }

    return false;
}

bool SceneGraphNode::isRelated(const SceneGraphNode& target) const {
    I64 targetGUID = target.getGUID();

    SGNRelationshipCache::RelationshipType type = _relationshipCache.clasifyNode(targetGUID);

    // We also ignore grandparents as this will usually be the root;
    return type != SGNRelationshipCache::RelationshipType::COUNT;
}

bool SceneGraphNode::isChild(const SceneGraphNode& target, bool recursive) const {
    I64 targetGUID = target.getGUID();

    SGNRelationshipCache::RelationshipType type = _relationshipCache.clasifyNode(targetGUID);
    if (type == SGNRelationshipCache::RelationshipType::GRANDCHILD && recursive) {
        return true;
    }

    return type == SGNRelationshipCache::RelationshipType::CHILD;
}

SceneGraphNode* SceneGraphNode::findChild(const U64 nameHash, bool sceneNodeName, bool recursive) const {
    SharedLock<SharedMutex> r_lock(_childLock);
    for (auto& child : _children) {
        const U64 cmpHash = sceneNodeName ? _ID(child->getNode().resourceName().c_str()) : _ID(child->name().c_str());
        if (cmpHash == nameHash) {
            return child;
        } else if (recursive) {
            SceneGraphNode* recChild = child->findChild(nameHash, sceneNodeName, recursive);
            if (recChild != nullptr) {
                return recChild;
            }
        }
    }

    // no children's name matches or there are no more children
    // so return nullptr, indicating that the node was not found yet
    return nullptr;
}

SceneGraphNode* SceneGraphNode::findChild(I64 GUID, bool sceneNodeGuid, bool recursive) const {
    SharedLock<SharedMutex> r_lock(_childLock);
    for (auto& child : _children) {
        if (sceneNodeGuid ? (child->getNode().getGUID() == GUID) : (child->getGUID() == GUID)) {
            return child;
        } else if (recursive) {
            SceneGraphNode* recChild = child->findChild(GUID, sceneNodeGuid, true);
            if (recChild != nullptr) {
                return recChild;
            }
        }
    }

    // no children's name matches or there are no more children
    // so return nullptr, indicating that the node was not found yet
    return nullptr;
}

bool SceneGraphNode::intersect(const Ray& ray, F32 start, F32 end, vectorEASTL<SGNRayResult>& intersections) const {
    // If we start from the root node, process children only
    if (_sceneGraph.getRoot() == *this) {
        forEachChild([&ray, start, end, &intersections](const SceneGraphNode* child, I32 /*childIdx*/) {
            child->intersect(ray, start, end, intersections);
            return true;
        });
    } else {
        // If we hit an AABB, we keep going down the graph
        const AABBRayResult result = get<BoundsComponent>()->getBoundingBox().intersect(ray, start, end);
        if (result.hit) {
            intersections.push_back({ getGUID(), result.dist, name().c_str() });
            forEachChild([&ray, start, end, &intersections](const SceneGraphNode* child, I32 /*childIdx*/) {
                child->intersect(ray, start, end, intersections);
                return true;
            });
        }
    }

    return !intersections.empty();
}

void SceneGraphNode::getOrderedNodeList(vectorEASTL<SceneGraphNode*>& nodeList) {
    // Compute from leaf to root to ensure proper calculations
    {
        SharedLock<SharedMutex> r_lock(_childLock);
        for (auto& child : _children) {
            child->getOrderedNodeList(nodeList);
        }
    }

    nodeList.push_back(this);
}

void SceneGraphNode::processDeleteQueue(vectorEASTL<size_t>& childList) {
    // See if we have any children to delete
    if (!childList.empty()) {
        UniqueLock<SharedMutex> w_lock(_childLock);
        for (size_t childIdx : childList) {
            _sceneGraph.destroySceneGraphNode(_children[childIdx]);
        }
        EraseIndices(_children, childList);
        _childCount.store(to_U32(_children.size()));
    }
}

/// Please call in MAIN THREAD! Nothing is thread safe here (for now) -Ionut
void SceneGraphNode::sceneUpdate(const U64 deltaTimeUS, SceneState& sceneState) {
    OPTICK_EVENT();

    // update local time
    _elapsedTimeUS += deltaTimeUS;
    _lastDeltaTimeUS = deltaTimeUS;

    if (hasFlag(Flags::ACTIVE)) {
        if (_lockToCamera != 0) {
            TransformComponent* tComp = get<TransformComponent>();
            if (tComp) {
                Camera* cam = Camera::findCamera(_lockToCamera);
                if (cam) {
                    cam->updateLookAt();
                    tComp->setOffset(true, cam->getWorldMatrix());
                }
            }
        }

        Attorney::SceneNodeSceneGraph::sceneUpdate(*_node, deltaTimeUS, *this, sceneState);
    }
}

void SceneGraphNode::processEvents() {
    OPTICK_EVENT();

    const ECS::EntityId id = GetEntityID();
    
    ECS::CustomEvent evt = {};
    while (_events.try_dequeue(evt)) {
        switch (evt._type) {
            case ECS::CustomEvent::Type::RelationshipCacheInvalidated: {
                if (!_relationshipCache.isValid()) {
                    _relationshipCache.rebuild();
                }
            } break;
            case ECS::CustomEvent::Type::EntityFlagChanged: {
                const Flags flag = static_cast<Flags>(std::any_cast<U32>(evt._flag));
                if (flag == Flags::SELECTED) {
                    RenderingComponent* rComp = get<RenderingComponent>();
                    if (rComp != nullptr) {
                        rComp->toggleRenderOption(RenderingComponent::RenderOptions::RENDER_SELECTION, std::any_cast<bool>(evt._userData));
                    }
                }
            } break;
            default: break;
        };

        _compManager->PassDataToAllComponents(id, evt);
    }
}

bool SceneGraphNode::prepareRender(RenderingComponent& rComp, const RenderStagePass& renderStagePass, const Camera& camera, bool refreshData) {
    OPTICK_EVENT();

    AnimationComponent* aComp = get<AnimationComponent>();
    if (aComp) {
        std::pair<vec2<U32>, ShaderBuffer*> data = aComp->getAnimationData();
        if (data.second != nullptr) {
            RenderPackage& pkg = rComp.getDrawPackage(renderStagePass);

            ShaderBufferBinding buffer = {};
            buffer._binding = ShaderBufferLocation::BONE_TRANSFORMS;
            buffer._buffer = data.second;
            buffer._elementRange = data.first;

            pkg.addShaderBuffer(0, buffer);
        }
    }
    
    return _node->prepareRender(*this, rComp, renderStagePass, camera, refreshData);
}

void SceneGraphNode::onRefreshNodeData(const RenderStagePass& renderStagePass, const Camera& camera, bool refreshData, GFX::CommandBuffer& bufferInOut) {
    _node->onRefreshNodeData(*this, renderStagePass, camera, refreshData, bufferInOut);
}

bool SceneGraphNode::getDrawState(RenderStagePass stagePass, U8 LoD) const {
    return _node->renderState().drawState(stagePass, LoD);
}

void SceneGraphNode::onNetworkSend(U32 frameCount) {
    forEachChild([frameCount](SceneGraphNode* child, I32 /*childIdx*/) {
        child->onNetworkSend(frameCount);
        return true;
    });

    NetworkingComponent* net = get<NetworkingComponent>();
    if (net) {
        net->onNetworkSend(frameCount);
    }
}


bool SceneGraphNode::preCullNode(const BoundsComponent& bounds, const NodeCullParams& params, F32& distanceToClosestPointSQ) const {
    OPTICK_EVENT();

    // If the node is still loading, DO NOT RENDER IT. Bad things happen :D
    if (!hasFlag(Flags::LOADING)) {
        // Get camera info
        const vec3<F32>& eye = params._currentCamera->getEye();

        // Check distance to sphere edge (center - radius)
        distanceToClosestPointSQ = bounds.getBoundingBox().nearestDistanceFromPointSquared(eye);
        if (distanceToClosestPointSQ < params._cullMaxDistanceSq) {
            const F32 upperBound = params._minExtents.maxComponent();
            if (upperBound > 0.0f &&
                bounds.getBoundingBox().getExtent().maxComponent() < upperBound)
            {
                return true;
            }

            RenderingComponent* rComp = get<RenderingComponent>();
            const vec2<F32>& renderRange = rComp->renderRange();
            if (IS_IN_RANGE_INCLUSIVE(distanceToClosestPointSQ, SIGNED_SQUARED(renderRange.min), SQUARED(renderRange.max))) {
                if (params._minLoD > -1) {
                    if (rComp->getLoDLevel(bounds, eye, params._stage, params._lodThresholds) > params._minLoD) {
                        return true;
                    }
                }

                return false;
            }
        }
    }

    return true;
}

bool SceneGraphNode::cullNode(const NodeCullParams& params,
                              Frustum::FrustCollision& collisionTypeOut,
                              F32& distanceToClosestPointSQ) const {
    OPTICK_EVENT();

    collisionTypeOut = Frustum::FrustCollision::FRUSTUM_OUT;

    // Some nodes should always render for different reasons (eg, trees are instanced and bound to the parent chunk)
    if (hasFlag(Flags::VISIBILITY_LOCKED)) {
        collisionTypeOut = Frustum::FrustCollision::FRUSTUM_IN;
        return false;
    }

    const BoundsComponent* bComp = get<BoundsComponent>();
    if (preCullNode(*bComp, params, distanceToClosestPointSQ)) {
        return true;
    }

    STUBBED("ToDo: make this work in a multi-threaded environment -Ionut");
    _frustPlaneCache = -1;
    I8 fakePlaneCache = -1;

    const BoundingSphere& sphere = bComp->getBoundingSphere();
    const BoundingBox& boundingBox = bComp->getBoundingBox();
    const F32 radius = sphere.getRadius();
    const vec3<F32>& center = sphere.getCenter();

    // Get camera info
    const vec3<F32>& eye = params._currentCamera->getEye();
    // Sphere is in range, so check bounds primitives againts the frustum
    if (!boundingBox.containsPoint(eye)) {
        const Frustum& frust = params._currentCamera->getFrustum();
        // Check if the bounding sphere is in the frustum, as Frustum <-> Sphere check is fast
        collisionTypeOut = frust.ContainsSphere(center, radius, fakePlaneCache);
        if (collisionTypeOut == Frustum::FrustCollision::FRUSTUM_INTERSECT) {
            // If the sphere is not completely in the frustum, check the AABB
            collisionTypeOut = frust.ContainsBoundingBox(boundingBox, fakePlaneCache);
        }
    } else {
        // We are inside the AABB. So ... intersect?
        collisionTypeOut = Frustum::FrustCollision::FRUSTUM_INTERSECT;
    }

    return collisionTypeOut == Frustum::FrustCollision::FRUSTUM_OUT;
}

void SceneGraphNode::invalidateRelationshipCache(SceneGraphNode* source) {
    if (source == this || !_relationshipCache.isValid()) {
        return;
    }

    SendEvent(
    {
        ECS::CustomEvent::Type::RelationshipCacheInvalidated
    });

    _relationshipCache.invalidate();

    if (_parent && _parent->parent()) {
        _parent->invalidateRelationshipCache(this);

        forEachChild([this, source](SceneGraphNode* child, I32 /*childIdx*/) {
            if (!source || child->getGUID() != source->getGUID()) {
                child->invalidateRelationshipCache(this);
            }
            return true;
        });
    }
}

bool SceneGraphNode::saveCache(ByteBuffer& outputBuffer) const {
    getNode().saveCache(outputBuffer);

    for (EditorComponent* editorComponent : Hacks._editorComponents) {
        if (!Attorney::EditorComponentSceneGraphNode::saveCache(*editorComponent, outputBuffer)) {
            return false;
        }
    }

    if (!_sceneGraph.GetECSManager().saveCache(*this, outputBuffer)) {
        return false;
    }

    return true;
}

bool SceneGraphNode::loadCache(ByteBuffer& inputBuffer) {
    getNode().loadCache(inputBuffer);

    for (EditorComponent* editorComponent : Hacks._editorComponents) {
        if (!Attorney::EditorComponentSceneGraphNode::loadCache(*editorComponent, inputBuffer)) {
            return false;
        }
    }

    if (!_sceneGraph.GetECSManager().loadCache(*this, inputBuffer)) {
        return false;
    }

    return true;
}

void SceneGraphNode::saveToXML(const Str256& sceneLocation, DELEGATE<void, const char*> msgCallback) const {
    if (!serialize()) {
        return;
    }

    if (msgCallback) {
        msgCallback(Util::StringFormat("Saving node [ %s ] ...", name().c_str()).c_str());
    }

    static boost::property_tree::xml_writer_settings<std::string> settings(' ', 4);

    boost::property_tree::ptree pt;
    pt.put("static", usageContext() == NodeUsageContext::NODE_STATIC);

    getNode().saveToXML(pt);

    for (EditorComponent* editorComponent : Hacks._editorComponents) {
        Attorney::EditorComponentSceneGraphNode::saveToXML(*editorComponent, pt);
    }

    stringImpl targetFile = sceneLocation + "/nodes/";
    targetFile.append(parent()->name());
    targetFile.append("_");
    targetFile.append(name());
    targetFile.append(".xml");
    write_xml(targetFile.c_str(), pt, std::locale(), settings);

    forEachChild([&sceneLocation, &msgCallback](const SceneGraphNode* child, I32 /*childIdx*/){
        child->saveToXML(sceneLocation, msgCallback);
        return true;
    });
}

void SceneGraphNode::loadFromXML(const boost::property_tree::ptree& pt) {
    if (!serialize()) {
        return;
    }

    changeUsageContext(pt.get("static", false) ? NodeUsageContext::NODE_STATIC : NodeUsageContext::NODE_DYNAMIC);

    U32 componentsToLoad = 0;
    for (U8 i = 1; i < to_U8(ComponentType::COUNT); ++i) {
        ComponentType type = ComponentType::_from_integral(1 << i);
        if (pt.count(type._to_string()) > 0) {
            componentsToLoad |= type._to_integral();
        }
    }

    if (componentsToLoad != 0) {
        AddComponents(componentsToLoad, false);
    }

    for (EditorComponent* editorComponent : Hacks._editorComponents) {
        Attorney::EditorComponentSceneGraphNode::loadFromXML(*editorComponent, pt);
    }
}

void SceneGraphNode::setFlag(Flags flag) noexcept {
    SetBit(_nodeFlags, to_U32(flag));
    if (PropagateFlagToChildren(flag)) {
        forEachChild([flag](SceneGraphNode* child, I32 /*childIdx*/) {
            child->setFlag(flag);
            return true;
        });
    }

    SendEvent(
    {
        ECS::CustomEvent::Type::EntityFlagChanged,
        true,
        to_U32(flag)
    });
}

void SceneGraphNode::clearFlag(Flags flag) noexcept {
    ClearBit(_nodeFlags, to_U32(flag));
    if (PropagateFlagToChildren(flag)) {
        forEachChild([flag](SceneGraphNode* child, I32 /*childIdx*/) {
            child->clearFlag(flag);
            return true;
        });
    }
    
    SendEvent(
    {
        ECS::CustomEvent::Type::EntityFlagChanged,
        false,
        to_U32(flag)
    });
}

bool SceneGraphNode::hasFlag(Flags flag) const noexcept {
    return BitCompare(_nodeFlags, to_U32(flag));
}

void SceneGraphNode::SendEvent(ECS::CustomEvent&& event) {
    if (_events.enqueue(std::move(event))) {
        //?
    }
}

void SceneGraphNode::SendEvent(const ECS::CustomEvent& event) {
    if (_events.enqueue(event)) {
        //Yay
    }
    //Nay
}
};