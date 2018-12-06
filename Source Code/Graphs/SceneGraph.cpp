#include "stdafx.h"

#include "Headers/SceneGraph.h"
#include "Core/Headers/Console.h"
#include "Utility/Headers/Localization.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Managers/Headers/SceneManager.h"
#include "Managers/Headers/FrameListenerManager.h"
#include "Rendering/RenderPass/Headers/RenderQueue.h"
#include "Geometry/Material/Headers/Material.h"

#include "ECS/Systems/Headers/ECSManager.h"
#include "ECS/Components/Headers/BoundsComponent.h"

namespace Divide {

namespace {
    U32 ignoredNodeType = to_base(SceneNodeType::TYPE_ROOT) |
                          to_base(SceneNodeType::TYPE_PARTICLE_EMITTER) |
                          to_base(SceneNodeType::TYPE_TRIGGER) |
                          to_base(SceneNodeType::TYPE_SKY) |
                          to_base(SceneNodeType::TYPE_VEGETATION_GRASS);
};

SceneGraph::SceneGraph(Scene& parentScene)
    : FrameListener(),
      SceneComponent(parentScene),
     _loadComplete(false),
     _octreeChanged(false),
     _ecsEngine(new ECS::ECSEngine())
{
    _ecsManager = std::make_unique<ECSManager>(parentScene.context(), GetECSEngine());

    REGISTER_FRAME_LISTENER(this, 1);

    SceneGraphNodeDescriptor rootDescriptor;
    rootDescriptor._name = "ROOT";
    rootDescriptor._node = std::make_shared<SceneNode>(parentScene.resourceCache(), GUIDWrapper::generateGUID(), "ROOT", SceneNodeType::TYPE_ROOT);
    rootDescriptor._componentMask = to_base(ComponentType::TRANSFORM) | to_base(ComponentType::BOUNDS);
    rootDescriptor._usageContext = NodeUsageContext::NODE_STATIC;

    _root = createSceneGraphNode(*this, rootDescriptor);

    _root->get<BoundsComponent>()->ignoreTransform(true);
    _root->postLoad();

    //Attorney::SceneNodeSceneGraph::postLoad(*rootSGN.getNode(), rootSGN);
    onNodeAdd(*_root);

    U32 octreeNodeMask = to_base(SceneNodeType::TYPE_ROOT) |
                         to_base(SceneNodeType::TYPE_SKY) |
                         to_base(SceneNodeType::TYPE_INFINITEPLANE) |
                         to_base(SceneNodeType::TYPE_VEGETATION_GRASS);

    _octree.reset(new Octree(octreeNodeMask));
    _octreeUpdating = false;
}

SceneGraph::~SceneGraph()
{ 
    _octree.reset();
    _allNodes.clear();
    UNREGISTER_FRAME_LISTENER(this);
    Console::d_printfn(Locale::get(_ID("DELETE_SCENEGRAPH")));
    // Should recursively delete the entire scene graph
    unload();
}

void SceneGraph::unload()
{
    idle();

    destroySceneGraphNode(_root);
    assert(_root == nullptr);
}

void SceneGraph::addToDeleteQueue(SceneGraphNode* node, vec_size childIdx) {
    UniqueLockShared w_lock(_pendingDeletionLock);
    vector<vec_size>& list = _pendingDeletion[node];
    if (std::find(std::cbegin(list), std::cend(list), childIdx) == std::cend(list))
    {
        list.push_back(childIdx);
    }
}

void SceneGraph::onNodeDestroy(SceneGraphNode& oldNode) {
    I64 guid = oldNode.getGUID();

    if (guid == _root->getGUID()) {
        return;
    }

    vectorEASTL<SceneGraphNode*>& nodesByType = _nodesByType[to_base(oldNode.getNode()->type())];

    nodesByType.erase(eastl::remove_if(eastl::begin(nodesByType), eastl::end(nodesByType), 
                                     [guid](SceneGraphNode* node)-> bool
                                     {
                                         return node && node->getGUID() == guid;
                                     }),
                      eastl::end(nodesByType));

    Attorney::SceneGraph::onNodeDestroy(_parentScene, oldNode);

    _allNodes.erase(std::remove_if(std::begin(_allNodes), std::end(_allNodes),
                                   [guid](SceneGraphNode* node)-> bool 
                                   {
                                       return node && node->getGUID() == guid;
                                   }),
                    std::end(_allNodes));
}

void SceneGraph::onNodeAdd(SceneGraphNode& newNode) {
    SceneGraphNode* newNodePtr = &newNode;

    _allNodes.push_back(newNodePtr);
    _nodesByType[to_base(newNodePtr->getNode()->type())].push_back(newNodePtr);
    
    if (_loadComplete) {
        WAIT_FOR_CONDITION(!_octreeUpdating);
        _octreeChanged = _octree->addNode(newNodePtr);
    }
}

void SceneGraph::onNodeTransform(SceneGraphNode& node) {
    if (_loadComplete) {
        node.setUpdateFlag(SceneGraphNode::UpdateFlag::SPATIAL_PARTITION);
    }
}

void SceneGraph::idle()
{
}

bool SceneGraph::removeNodesByType(SceneNodeType nodeType) {
    return _root != nullptr && getRoot().removeNodesByType(nodeType);
}

bool SceneGraph::removeNode(I64 guid) {
    return removeNode(findNode(guid));
}

bool SceneGraph::removeNode(SceneGraphNode* node) {
    if (node) {
        SceneGraphNode* parent = node->getParent();
        if (parent) {
            if (!parent->removeNode(*node)) {
                return false;
            }
        }

        return true;
    }

    return false;
}

bool SceneGraph::frameStarted(const FrameEvent& evt) {
    UniqueLockShared lock(_pendingDeletionLock);
    if (!_pendingDeletion.empty()) {
        for (auto entry : _pendingDeletion) {
            if (entry.first != nullptr) {
                entry.first->processDeleteQueue(entry.second);
            }
        }
        _pendingDeletion.clear();
    }

    // Gather all nodes in order
    _orderedNodeList.resize(0);
    _root->getOrderedNodeList(_orderedNodeList);

    for (SceneGraphNode* node : _orderedNodeList) {
        node->frameStarted();
    }

    return true;
}

bool SceneGraph::frameEnded(const FrameEvent& evt) {
    for (SceneGraphNode* node : _orderedNodeList) {
        node->frameEnded();
    }

    return true;
}

void SceneGraph::sceneUpdate(const U64 deltaTimeUS, SceneState& sceneState) {

    F32 msTime = Time::MicrosecondsToMilliseconds<F32>(deltaTimeUS);
    GetECSEngine().PreUpdate(msTime);
    GetECSEngine().Update(msTime);
    GetECSEngine().PostUpdate(msTime);


    for (SceneGraphNode* node : _orderedNodeList) {
        node->sceneUpdate(deltaTimeUS, sceneState);
    }

    if (_loadComplete && false) {
        CreateTask(parentScene().context(),
            [this, deltaTimeUS](const Task& parentTask) mutable
            {
                _octreeUpdating = true;
                if (_octreeChanged) {
                    _octree->updateTree();
                }
                _octree->update(deltaTimeUS);
            }).startTask(TaskPriority::REALTIME,
                [this]() mutable
                {
                    _octreeUpdating = false;
                });
    }
}

void SceneGraph::onStartUpdateLoop(const U8 loopNumber) {
    ACKNOWLEDGE_UNUSED(loopNumber);

    GetECSEngine().OnUpdateLoop();
}

void SceneGraph::onCameraUpdate(const Camera& camera) {
    _root->onCameraUpdate(camera.getGUID(), camera.getEye(), camera.getViewMatrix());
}

void SceneGraph::onNetworkSend(U32 frameCount) {
    _root->onNetworkSend(frameCount);
}

void SceneGraph::intersect(const Ray& ray, F32 start, F32 end, vector<SGNRayResult>& intersections) const {
    _root->intersect(ray, start, end, intersections);

    /*if (_loadComplete) {
        WAIT_FOR_CONDITION(!_octreeUpdating);
        U32 filter = to_base(SceneNodeType::TYPE_OBJECT3D);
        SceneGraphNode* collision = _octree->nearestIntersection(ray, start, end, filter)._intersectedObject1.lock();
        if (collision) {
            Console::d_printfn("Octree ray cast [ %s ]", collision->name().c_str());
        }
    }*/
}

void SceneGraph::postLoad() {
    _octree->addNodes(_allNodes);
    _octreeChanged = true;
    _loadComplete = true;
}

SceneGraphNode* SceneGraph::createSceneGraphNode(SceneGraph& sceneGraph, const SceneGraphNodeDescriptor& descriptor) {
    UniqueLock u_lock(_nodeCreateMutex);

    ECS::EntityId nodeID = GetEntityManager()->CreateEntity<SceneGraphNode>(sceneGraph, descriptor);
    return static_cast<SceneGraphNode*>(GetEntityManager()->GetEntity(nodeID));
}

void SceneGraph::destroySceneGraphNode(SceneGraphNode*& node, bool inPlace) {
    if (node) {
        if (inPlace) {
            GetEntityManager()->DestroyAndRemoveEntity(node->GetEntityID());
        } else {
            GetEntityManager()->DestroyEntity(node->GetEntityID());
        }
        node = nullptr;
    }
}

const vectorEASTL<SceneGraphNode*>& SceneGraph::getNodesByType(SceneNodeType type) const {
    return _nodesByType[to_base(type)];
}

ECS::EntityManager* SceneGraph::GetEntityManager() {
    return GetECSEngine().GetEntityManager();
}

ECS::EntityManager* SceneGraph::GetEntityManager() const {
    return GetECSEngine().GetEntityManager();
}

ECS::ComponentManager* SceneGraph::GetComponentManager() {
    return GetECSEngine().GetComponentManager();
}

ECS::ComponentManager* SceneGraph::GetComponentManager() const {
    return GetECSEngine().GetComponentManager();
}

bool SceneGraph::saveCache(ByteBuffer& outputBuffer) const {
    return _root->saveCache(outputBuffer);
}

bool SceneGraph::loadCache(ByteBuffer& inputBuffer) {
    return _root->loadCache(inputBuffer);
}

namespace {
    boost::property_tree::ptree dumpSGNtoAssets(const SceneGraphNode& node) {
        boost::property_tree::ptree entry;
        entry.put("<xmlattr>.name", node.name());
        entry.put("<xmlattr>.type", node.getNode()->getTypeName());

        node.forEachChild([&entry](const SceneGraphNode& child) {
            entry.add_child("node", dumpSGNtoAssets(child));
        });

        return entry;
    }
};

void SceneGraph::saveToXML() const {
    const stringImpl& scenePath = Paths::g_xmlDataLocation + Paths::g_scenesLocation;
    const boost::property_tree::xml_writer_settings<std::string> settings(' ', 4);

    Console::printfn(Locale::get(_ID("XML_SAVE_SCENE")), parentScene().resourceName().c_str());
    stringImpl sceneLocation(scenePath + "/" + parentScene().resourceName());

    {
        boost::property_tree::ptree pt;
        pt.add_child("entities.node", dumpSGNtoAssets(getRoot()));

        copyFile(sceneLocation + "/", "assets.xml", sceneLocation + "/", "assets.xml.bak", true);
        write_xml((sceneLocation + "/" + "assets.xml").c_str(), pt, std::locale(), settings);
    }

    getRoot().forEachChild([&sceneLocation](const SceneGraphNode& child) {
        child.saveToXML(sceneLocation);
    });
}

void SceneGraph::loadFromXML() {

}
};