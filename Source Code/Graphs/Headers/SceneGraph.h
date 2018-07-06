/*
   Copyright (c) 2015 DIVIDE-Studio
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

#ifndef _SCENE_GRAPH_H_
#define _SCENE_GRAPH_H_

#include "SceneGraphNode.h"
#include "Core/Headers/Console.h"
#include "Utility/Headers/Localization.h"

namespace Divide {
class Ray;
class Scene;
class SceneState;

namespace Attorney {
    class SceneGraphSGN;
};

class SceneGraph : private NonCopyable {
    friend class Attorney::SceneGraphSGN;
   public:
    SceneGraph();
    ~SceneGraph();

    inline SceneGraphNode_ptr getRoot() const {
        return _root;
    }

    inline SceneGraphNode_wptr findNode(const stringImpl& name, bool sceneNodeName = false) {
        return _root->findNode(name, sceneNodeName);
    }

    /// Update all nodes. Called from "updateSceneState" from class Scene
    void sceneUpdate(const U64 deltaTime, SceneState& sceneState);

    void idle();

    void intersect(const Ray& ray, F32 start, F32 end, vectorImpl<SceneGraphNode_wptr>& selectionHits);

    void deleteNode(SceneGraphNode_wptr node, bool deleteOnAdd);

   protected:
    void onNodeDestroy(SceneGraphNode& oldNode);

   private:
    SceneGraphNode_ptr _root;
    vectorImpl<SceneGraphNode_wptr> _pendingDeletionNodes;
};

namespace Attorney {
class SceneGraphSGN {
   private:
    static void onNodeDestroy(SceneGraph& sceneGraph, SceneGraphNode& oldNode) {
        sceneGraph.onNodeDestroy(oldNode);
    }

    friend class Divide::SceneGraphNode;
};

};  // namespace Attorney

};  // namespace Divide
#endif