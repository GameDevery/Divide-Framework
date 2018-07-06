/*
   Copyright (c) 2016 DIVIDE-Studio
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

#ifndef _RENDER_PASS_CULLER_H_
#define _RENDER_PASS_CULLER_H_

#include "Platform/Video/Headers/RenderAPIEnums.h"
#include "Platform/Headers/PlatformDefines.h"

/// This class performs all the necessary visibility checks on the scene's
/// scenegraph to decide what get's rendered and what not
namespace Divide {
class SceneState;
class SceneRenderState;

class Task;
class Camera;
class SceneGraph;

FWD_DECLARE_MANAGED_CLASS(SceneGraphNode);

class RenderPassCuller {
   public:
    // draw order, node pointer
    typedef std::pair<U32, SceneGraphNode_cwptr> VisibleNode;
    typedef vectorImpl<VisibleNode> VisibleNodeList;

    //Should return true if the node is not inside the frustum
    typedef std::function<bool(const SceneGraphNode&)> CullingFunction;

   public:
    RenderPassCuller();
    ~RenderPassCuller();

    // flush all caches
    void clear();

    VisibleNodeList& getNodeCache(RenderStage stage);
    const VisibleNodeList& getNodeCache(RenderStage stage) const;

    void frustumCull(SceneGraph& sceneGraph,
                     const SceneState& sceneState,
                     RenderStage stage,
                     bool async,
                     const CullingFunction& cullingFunction);

    bool wasNodeInView(I64 GUID, RenderStage stage) const;

   protected:

    void frumstumPartitionCuller(const Task& parentTask,
                                 U32 start,
                                 U32 end,
                                 SceneGraphNode& root,
                                 const Camera& camera,
                                 RenderStage stage,
                                 F32 cullMaxDistance);
    // return true if the node is not currently visible
    void frustumCullNode(const Task& parentTask,
                         const SceneGraphNode& node,
                         const Camera& currentCamera,
                         RenderStage currentStage,
                         F32 cullMaxDistance,
                         U32 nodeListIndex,
                         bool clearList);
    void addAllChildren(const SceneGraphNode& currentNode,
                        RenderStage currentStage,
                        VisibleNodeList& nodes);

    U32 stageToCacheIndex(RenderStage stage) const;

   protected:
    std::array<CullingFunction, to_const_uint(RenderStage::COUNT)> _cullingFunction;
    std::array<VisibleNodeList, to_const_uint(RenderStage::COUNT)> _visibleNodes;
    std::array<vectorImpl<VisibleNodeList>, to_const_uint(RenderStage::COUNT)> _perThreadNodeList;
};

};  // namespace Divide
#endif
