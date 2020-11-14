#include "stdafx.h"

#include "Headers/SceneGraph.h"
#include "Core/Headers/ByteBuffer.h"
#include "Core/Headers/EngineTaskPool.h"
#include "Core/Headers/Kernel.h"
#include "Geometry/Material/Headers/Material.h"
#include "Managers/Headers/FrameListenerManager.h"
#include "Managers/Headers/SceneManager.h"
#include "Utility/Headers/Localization.h"

#include "ECS/Systems/Headers/ECSManager.h"

namespace Divide {

namespace {
    constexpr I8 g_cacheMarkerByteValue = -126;
    constexpr U32 g_nodesPerPartition = 32u;
};

SceneGraph::SceneGraph(Scene& parentScene)
    : FrameListener("SceneGraph", parentScene.context().kernel().frameListenerMgr(), 1),
      SceneComponent(parentScene),
     _loadComplete(false),
     _octreeChanged(false),
     _nodeListChanged(false),
     _root(nullptr)
{
    _ecsManager = eastl::make_unique<ECSManager>(parentScene.context(), GetECSEngine());

    SceneGraphNodeDescriptor rootDescriptor = {};
    rootDescriptor._name = "ROOT";
    rootDescriptor._node = std::make_shared<SceneNode>(parentScene.resourceCache(), generateGUID(), "ROOT", ResourcePath{ "ROOT" }, ResourcePath{ "" }, SceneNodeType::TYPE_TRANSFORM, to_base(ComponentType::TRANSFORM) | to_base(ComponentType::BOUNDS));
    rootDescriptor._componentMask = to_base(ComponentType::TRANSFORM) | to_base(ComponentType::BOUNDS);
    rootDescriptor._usageContext = NodeUsageContext::NODE_STATIC;

    _root = createSceneGraphNode(this, rootDescriptor);
    _root->postLoad();
    onNodeAdd(_root);

    constexpr U16 octreeNodeMask = to_base(SceneNodeType::TYPE_TRANSFORM) |
                                   to_base(SceneNodeType::TYPE_SKY) |
                                   to_base(SceneNodeType::TYPE_INFINITEPLANE) |
                                   to_base(SceneNodeType::TYPE_VEGETATION);

    _octree.reset(new Octree(octreeNodeMask));
    _octreeUpdating = false;
}

SceneGraph::~SceneGraph()
{ 
    _octree.reset();
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

void SceneGraph::addToDeleteQueue(SceneGraphNode* node, size_t childIdx) {
    UniqueLock<SharedMutex> w_lock(_pendingDeletionLock);
    vectorEASTL<size_t>& list = _pendingDeletion[node];
    if (eastl::find(cbegin(list), cend(list), childIdx) == cend(list))
    {
        list.push_back(childIdx);
    }
}

void SceneGraph::onNodeDestroy(SceneGraphNode* oldNode) {
    I64 guid = oldNode->getGUID();

    if (guid == _root->getGUID()) {
        return;
    }

    erase_if(_nodesByType[to_base(oldNode->getNode().type())],
             [guid](SceneGraphNode* node)-> bool
             {
                 return node && node->getGUID() == guid;
             });

    Attorney::SceneGraph::onNodeDestroy(_parentScene, oldNode);

    _nodeListChanged = true;
}

void SceneGraph::onNodeAdd(SceneGraphNode* newNode) {
    _nodesByType[to_base(newNode->getNode().type())].push_back(newNode);

    _nodeListChanged = true;

    if (_loadComplete) {
        WAIT_FOR_CONDITION(!_octreeUpdating);
        _octreeChanged = _octree->addNode(newNode);
    }
}

void SceneGraph::onNodeTransform(SceneGraphNode* node) {
    if (_loadComplete) {
        node->setFlag(SceneGraphNode::Flags::SPATIAL_PARTITION_UPDATE_QUEUED);
    }
}

void SceneGraph::idle()
{
}

bool SceneGraph::removeNodesByType(SceneNodeType nodeType) {
    return _root != nullptr && getRoot()->removeNodesByType(nodeType);
}

bool SceneGraph::removeNode(I64 guid) {
    return removeNode(findNode(guid));
}

bool SceneGraph::removeNode(SceneGraphNode* node) {
    if (node) {
        SceneGraphNode* parent = node->parent();
        if (parent) {
            if (!parent->removeChildNode(node, true)) {
                return false;
            }
        }

        return true;
    }

    return false;
}

bool SceneGraph::frameStarted(const FrameEvent& evt) {
    Attorney::SceneGraphNodeSceneGraph::frameStarted(getRoot(), evt);

    {
        UniqueLock<SharedMutex> lock(_pendingDeletionLock);
        if (!_pendingDeletion.empty()) {
            for (auto entry : _pendingDeletion) {
                if (entry.first != nullptr) {
                    Attorney::SceneGraphNodeSceneGraph::processDeleteQueue(entry.first, entry.second);
                }
            }
            _pendingDeletion.clear();
        }
    }

    // Gather all nodes at the start of the frame only if we added/removed any of them
    if (_nodeListChanged) {
        // Very rarely called
        _nodeList.resize(0);
        Attorney::SceneGraphNodeSceneGraph::getAllNodes(_root, _nodeList);
        _nodeListChanged = false;
    }

    return true;
}

void SceneGraph::sceneUpdate(const U64 deltaTimeUS, SceneState& sceneState) {
    OPTICK_EVENT();

    const F32 msTime = Time::MicrosecondsToMilliseconds<F32>(deltaTimeUS);
    {
        OPTICK_EVENT("ECS::PreUpdate");
        GetECSEngine().PreUpdate(msTime);
    }
    {
        OPTICK_EVENT("ECS::Update");
        GetECSEngine().Update(msTime);
    }
    {
        OPTICK_EVENT("ECS::PostUpdate");
        GetECSEngine().PostUpdate(msTime);
    }

    PlatformContext& context = parentScene().context();

    ParallelForDescriptor descriptor = {};
    descriptor._iterCount = to_U32(_nodeList.size());
    descriptor._partitionSize = g_nodesPerPartition;
    descriptor._cbk = [this](const Task* /*parentTask*/, const U32 start, const U32 end) {
        for (U32 i = start; i < end; ++i) {
            Attorney::SceneGraphNodeSceneGraph::processEvents(_nodeList[i]);
        }
    };

    parallel_for(context, descriptor);

    for (SceneGraphNode* node : _nodeList) {
        node->sceneUpdate(deltaTimeUS, sceneState);
    }

    if (_loadComplete) {
        Start(*CreateTask(context,
            [this, deltaTimeUS](const Task& /*parentTask*/) mutable
            {
                OPTICK_EVENT("Octree Update");
                _octreeUpdating = true;
                if (_octreeChanged) {
                    _octree->updateTree();
                }
                _octree->update(deltaTimeUS);
            }), 
            //TaskPriority::DONT_CARE,
            TaskPriority::REALTIME,
            [this]() { _octreeUpdating = false; });
    }
}

void SceneGraph::onStartUpdateLoop(const U8 loopNumber) {
    ACKNOWLEDGE_UNUSED(loopNumber);
}

void SceneGraph::onNetworkSend(U32 frameCount) {
    Attorney::SceneGraphNodeSceneGraph::onNetworkSend(_root, frameCount);
}

bool SceneGraph::intersect(const SGNIntersectionParams& params, vectorEASTL<SGNRayResult>& intersectionsOut) const {
    intersectionsOut.resize(0);
    return _root->intersect(params, intersectionsOut);

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
    for (const auto& nodes : _nodesByType) {
        _octree->addNodes(nodes);
    }
    _octreeChanged = true;
    _loadComplete = true;
}

SceneGraphNode* SceneGraph::createSceneGraphNode(SceneGraph* sceneGraph, const SceneGraphNodeDescriptor& descriptor) {
    UniqueLock<Mutex> u_lock(_nodeCreateMutex);

    const ECS::EntityId nodeID = GetEntityManager()->CreateEntity<SceneGraphNode>(sceneGraph, descriptor);
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

size_t SceneGraph::getTotalNodeCount() const noexcept {
    size_t ret = 0;
    for (const auto& nodes : _nodesByType) {
        ret += nodes.size();
    }

    return ret;
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

SceneGraphNode* SceneGraph::findNode(const Str128& name, bool sceneNodeName) const {
    return findNode(_ID(name.c_str()), sceneNodeName);
}

SceneGraphNode* SceneGraph::findNode(const U64 nameHash, bool sceneNodeName) const {
    const U64 cmpHash = sceneNodeName ? _ID(_root->getNode().resourceName().c_str()) : _ID(_root->name().c_str());

    if (cmpHash == nameHash) {
        return _root;
    }

    return _root->findChild(nameHash, sceneNodeName, true);
}

SceneGraphNode* SceneGraph::findNode(I64 guid) const {
    if (_root->getGUID() == guid) {
        return _root;
    }

    return _root->findChild(guid, false, true);
}

bool SceneGraph::saveCache(ByteBuffer& outputBuffer) const {
    const bool ret = saveCache(_root, outputBuffer);
    outputBuffer << _ID(_root->name().c_str());
    return ret;
}

bool SceneGraph::loadCache(ByteBuffer& inputBuffer) {
    const U64 rootID = _ID(_root->name().c_str());
    U64 nodeID = 0u;
    I8 marker1 = -1, marker2 = -1;

    bool skipRoot = true;
    bool missingData = false;
    do {
        inputBuffer >> nodeID;
        if (nodeID == rootID && !skipRoot) {
            break;
        }

        SceneGraphNode* node = findNode(nodeID, false);

        if (node != nullptr && loadCache(node, inputBuffer)) {
            inputBuffer >> marker1;
            inputBuffer >> marker2;
            assert(marker1 == g_cacheMarkerByteValue && marker1 == marker2);
        } else {
            missingData = true;
            while(true) {
                inputBuffer >> marker1;
                if (marker1 == g_cacheMarkerByteValue) {
                    inputBuffer >> marker2;
                    if (marker2 == g_cacheMarkerByteValue) {
                        break;
                    }
                }
            }
        }
        if (nodeID == rootID && skipRoot) {
            skipRoot = false;
            nodeID = 0u;
        }
    } while (nodeID != rootID);

    return !missingData;
}

bool SceneGraph::saveCache(const SceneGraphNode* sgn, ByteBuffer& outputBuffer) const {
    // Because loading is async, nodes will not be necessarily in the same order. We need a way to find
    // the node using some sort of ID. Name based ID is bad, but is the only system available at the time of writing -Ionut
    outputBuffer << _ID(sgn->name().c_str());
    if (!Attorney::SceneGraphNodeSceneGraph::saveCache(sgn, outputBuffer)) {
        NOP();
    }
    // Data may be bad, so add markers to be able to just jump over the entire node data instead of attempting partial loads
    outputBuffer << g_cacheMarkerByteValue;
    outputBuffer << g_cacheMarkerByteValue;

    return sgn->forEachChild([this, &outputBuffer](SceneGraphNode* child, I32 /*idx*/) {
        return saveCache(child, outputBuffer);
    });
}

bool SceneGraph::loadCache(SceneGraphNode* sgn, ByteBuffer& inputBuffer) {
    return Attorney::SceneGraphNodeSceneGraph::loadCache(sgn, inputBuffer);
}

namespace {
    constexpr size_t g_sceneGraphVersion = 1;

    boost::property_tree::ptree dumpSGNtoAssets(const SceneGraphNode* node) {
        boost::property_tree::ptree entry;
        entry.put("<xmlattr>.name", node->name().c_str());
        entry.put("<xmlattr>.type", node->getNode().getTypeName().c_str());

        node->forEachChild([&entry](const SceneGraphNode* child, I32 /*childIdx*/) {
            if (child->serialize()) {
                entry.add_child("node", dumpSGNtoAssets(child));
            }
            return true;
        });

        return entry;
    }
};

void SceneGraph::saveToXML(const char* assetsFile, DELEGATE<void, std::string_view> msgCallback) const {
    const ResourcePath scenePath = Paths::g_xmlDataLocation + Paths::g_scenesLocation;
    ResourcePath sceneLocation(scenePath + "/" + parentScene().resourceName());

    {
        boost::property_tree::ptree pt;
        pt.put("version", g_sceneGraphVersion);
        pt.add_child("entities.node", dumpSGNtoAssets(getRoot()));

        if (copyFile(sceneLocation + "/", ResourcePath(assetsFile), sceneLocation + "/", ResourcePath("assets.xml.bak"), true)) {
            XML::writeXML((sceneLocation + "/" + assetsFile).str(), pt);
        } else {
            DIVIDE_UNEXPECTED_CALL();
        }
    }

    getRoot()->forEachChild([&sceneLocation, &msgCallback](const SceneGraphNode* child, I32 /*childIdx*/) {
        child->saveToXML(sceneLocation.str(), msgCallback);
        return true;
    });
}

namespace {
    boost::property_tree::ptree g_emptyPtree;
}

void SceneGraph::loadFromXML(const char* assetsFile) {
    using boost::property_tree::ptree;
    static const auto& scenePath = Paths::g_xmlDataLocation + Paths::g_scenesLocation;

    const ResourcePath file = scenePath + "/" + parentScene().resourceName() + "/" + assetsFile;

    if (!fileExists(file)) {
        return;
    }

    Console::printfn(Locale::get(_ID("XML_LOAD_GEOMETRY")), file.c_str());

    ptree pt = {};
    XML::readXML(file.str(), pt);
    if (pt.get("version", g_sceneGraphVersion) != g_sceneGraphVersion) {
        // ToDo: Scene graph version mismatch. Handle condition - Ionut
        NOP();
    }

    const auto readNode = [](const ptree& rootNode, XML::SceneNode& graphOut, auto& readNodeRef) -> void {
        for (const auto&[name, value] : rootNode.get_child("<xmlattr>", g_emptyPtree)) {
            if (name == "name") {
                graphOut.name = value.data();
            } else if (name == "type") {
                graphOut.typeHash = _ID(value.data().c_str());
            } else {
                //ToDo: Error handling -Ionut
                NOP();
            }
        }

        for (const auto&[name, ptree] : rootNode.get_child("")) {
            if (name == "node") {
                graphOut.children.emplace_back();
                readNodeRef(ptree, graphOut.children.back(), readNodeRef);
            }
        }
    };



    XML::SceneNode rootNode = {};
    const auto& [name, node_pt] = pt.get_child("entities", g_emptyPtree).front();
    // This is way faster than pre-declaring a std::function and capturing that or by using 2 separate
    // lambdas and capturing one.
    readNode(node_pt, rootNode, readNode);
    // This may not be needed;
    assert(rootNode.typeHash == _ID("TRANSFORM"));
    parentScene().addSceneGraphToLoad(rootNode);
}

bool SceneGraph::saveNodeToXML(const SceneGraphNode* node) const {
    const ResourcePath scenePath = Paths::g_xmlDataLocation + Paths::g_scenesLocation;
    const ResourcePath sceneLocation(scenePath + "/" + parentScene().resourceName());
    node->saveToXML(sceneLocation.str());
    return true;
}

bool SceneGraph::loadNodeFromXML(const char* assetsFile, SceneGraphNode* node) const {
    ACKNOWLEDGE_UNUSED(assetsFile);

    const ResourcePath scenePath = Paths::g_xmlDataLocation + Paths::g_scenesLocation;
    const ResourcePath sceneLocation(scenePath + "/" + parentScene().resourceName());
    node->loadFromXML(sceneLocation.str());
    return true;
}

};