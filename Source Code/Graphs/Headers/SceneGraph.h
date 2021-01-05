/*
   Copyright (c) 2018 DIVIDE-Studio
   Copyright (c) 2009 Ionut Cava

   This file is part of DIVIDE Framework.

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software
   and associated documentation files (the "Software"), to deal in the Software
   without restriction,
   including without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED,
   INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
   PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
   DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
   IN CONNECTION WITH THE SOFTWARE
   OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

#pragma once
#ifndef _SCENE_GRAPH_H_
#define _SCENE_GRAPH_H_

#include "Octree.h"
#include "Scenes/Headers/SceneComponent.h"
#include "Rendering/Headers/FrameListener.h"

namespace ECS {
    class ECSEngine;
};

namespace Divide {
class SceneState;
class ECSManager;

struct Ray;

namespace Attorney {
    class SceneGraphSGN;
};

class SceneGraph final : NonCopyable,
                         public FrameListener,
                         public SceneComponent
{

    friend class Attorney::SceneGraphSGN;

   public:
    explicit SceneGraph(Scene& parentScene);
    ~SceneGraph();

    void unload();

    const SceneGraphNode* getRoot() const noexcept { return _root; }
    SceneGraphNode* getRoot() noexcept { return _root; }

    SceneGraphNode* findNode(const Str128& name, bool sceneNodeName = false) const;
    SceneGraphNode* findNode(U64 nameHash, bool sceneNodeName = false) const;
    SceneGraphNode* findNode(I64 guid) const;

    Octree& getOctree() noexcept { return *_octree; }

    const Octree& getOctree() const noexcept { return *_octree; }

    /// Update all nodes. Called from "updateSceneState" from class Scene
    void sceneUpdate(U64 deltaTimeUS, SceneState& sceneState);
    void onStartUpdateLoop(U8 loopNumber);

    bool intersect(const SGNIntersectionParams& params, vectorEASTL<SGNRayResult>& intersectionsOut) const;

    SceneGraphNode* createSceneGraphNode(SceneGraph* sceneGraph, const SceneGraphNodeDescriptor& descriptor);

    void destroySceneGraphNode(SceneGraphNode*& node, bool inPlace = true);
    void addToDeleteQueue(SceneGraphNode* node, size_t childIdx);

    // If this function returns true, the node was successfully removed (or queued for removal)
    bool removeNode(I64 guid);
    bool removeNode(SceneGraphNode* node);
    // If this function returns true, nodes of the specified type were successfully removed (or queued for removal)
    bool removeNodesByType(SceneNodeType nodeType);

    const vectorEASTL<SceneGraphNode*>& getNodesByType(SceneNodeType type) const;

    void getNodesByType(std::initializer_list<SceneNodeType> types, vectorEASTL<SceneGraphNode*>& nodesOut) const {
        nodesOut.resize(0);
        for (const SceneNodeType type : types) {
            const vectorEASTL<SceneGraphNode*>& nodes = getNodesByType(type);
            nodesOut.insert(cend(nodesOut), cbegin(nodes), cend(nodes));
        }
    }

    size_t getTotalNodeCount() const noexcept;

    void onNetworkSend(U32 frameCount);

    void postLoad();

    bool saveNodeToXML(const SceneGraphNode* node) const;
    bool loadNodeFromXML(const char* assetsFile, SceneGraphNode* node) const;

    void saveToXML(const char* assetsFile, DELEGATE<void, std::string_view> msgCallback) const;
    void loadFromXML(const char* assetsFile);

    ECSManager& GetECSManager() noexcept { return *_ecsManager; }
    const ECSManager& GetECSManager() const noexcept { return *_ecsManager; }

    ECS::EntityManager* GetEntityManager();
    ECS::EntityManager* GetEntityManager() const;
    ECS::ComponentManager* GetComponentManager();
    ECS::ComponentManager* GetComponentManager() const;

    bool saveCache(ByteBuffer& outputBuffer) const;
    bool loadCache(ByteBuffer& inputBuffer);

    ECS::ECSEngine& GetECSEngine() noexcept { return _ecsEngine; }
    const ECS::ECSEngine& GetECSEngine() const noexcept { return _ecsEngine; }

   protected:
    void onNodeMoved(const SceneGraphNode& node);
    void onNodeDestroy(SceneGraphNode* oldNode);
    void onNodeAdd(SceneGraphNode* newNode);

    bool frameStarted(const FrameEvent& evt) override;

    bool saveCache(const SceneGraphNode* sgn, ByteBuffer& outputBuffer) const;
    bool loadCache(SceneGraphNode* sgn, ByteBuffer& inputBuffer);

   private:
    ECS::ECSEngine _ecsEngine;
    eastl::unique_ptr<ECSManager> _ecsManager;

    bool _loadComplete;
    bool _octreeChanged;
    bool _nodeListChanged;

    SceneGraphNode* _root;
    std::shared_ptr<Octree> _octree;
    std::atomic_bool _octreeUpdating;
    vectorEASTL<SceneGraphNode*> _nodeList;

    std::array<vectorEASTL<SceneGraphNode*>, to_base(SceneNodeType::COUNT)> _nodesByType;

    mutable Mutex _nodeCreateMutex;
    mutable SharedMutex _pendingDeletionLock;
    hashMap<SceneGraphNode*, vectorEASTL<size_t>> _pendingDeletion;
};

namespace Attorney {
class SceneGraphSGN {
    static void onNodeAdd(SceneGraph* sceneGraph, SceneGraphNode* newNode) {
        sceneGraph->onNodeAdd(newNode);
    }

    static void onNodeDestroy(SceneGraph* sceneGraph, SceneGraphNode* oldNode) {
        sceneGraph->onNodeDestroy(oldNode);
    }

    static void onNodeMoved(SceneGraph* sceneGraph, const SceneGraphNode& node) {
        sceneGraph->onNodeMoved(node);
    }

    friend class Divide::SceneGraphNode;
};

};  // namespace Attorney

};  // namespace Divide
#endif