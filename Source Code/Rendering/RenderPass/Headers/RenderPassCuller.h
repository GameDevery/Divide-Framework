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
#ifndef _RENDER_PASS_CULLER_H_
#define _RENDER_PASS_CULLER_H_

#include "Platform/Video/Headers/RenderAPIEnums.h"
#include "Platform/Video/Headers/ClipPlanes.h"

/// This class performs all the necessary visibility checks on the scene's
/// SceneGraph to decide what get's rendered and what not
namespace Divide {

class SceneState;
class SceneRenderState;

struct Task;
class Camera;
class SceneNode;
class SceneGraph;
class SceneGraphNode;
class PlatformContext;
enum class RenderStage : U8;

struct NodeCullParams {
    FrustumClipPlanes _clippingPlanes;
    vec4<U16> _lodThresholds = {1000u};
    vec3<F32> _minExtents = { 0.0f };
    std::pair<I64*, size_t> _ignoredGUIDS;
    const Camera* _currentCamera = nullptr;
    F32 _cullMaxDistanceSq = 0.0f;
    I32 _maxLoD = -1;
    RenderStage _stage = RenderStage::COUNT;
    bool _cullAllDynamicNodes = false;
    bool _cullAllStaticNodes = false;
};

struct VisibleNode {
    SceneGraphNode* _node = nullptr;
    F32 _distanceToCameraSq = 0.0f;
    bool _materialReady = true;
};

struct FeedBackContainer
{
    vectorEASTL<VisibleNode> _visibleNodes;
};

template<size_t N = Config::MAX_VISIBLE_NODES>
struct VisibleNodeList
{
    using Container = std::array<VisibleNode, N>;

    void append(const VisibleNodeList& other) {
        assert(_index + other._index < _nodes.size());

        std::memcpy(_nodes.data() + _index, other._nodes.data(), other._index * sizeof(VisibleNode));
        _index += other._index;
    }

    void append(const VisibleNode& node) {
        _nodes[_index.fetch_add(1)] = node;
    }
                  void      reset()       noexcept { _index.store(0); }
    [[nodiscard]] size_t    size()  const noexcept { return _index.load(); }
    [[nodiscard]] bufferPtr data()  const noexcept { return (bufferPtr)_nodes.data(); }

    [[nodiscard]] const VisibleNode& node(size_t idx) const noexcept { 
        assert(idx < _index.load());
        return _nodes[idx]; 
    }

    [[nodiscard]] VisibleNode& node(size_t idx) noexcept {
        assert(idx < _index.load());
        return _nodes[idx];
    }
private:
    Container _nodes;
    std::atomic_size_t _index = 0;
};

class RenderPassCuller {
    public:
        RenderPassCuller() = default;
        ~RenderPassCuller() = default;

        static bool onStartup();
        static bool onShutdown();

        void clear() noexcept;

        VisibleNodeList<>& frustumCull(const NodeCullParams& params, const SceneGraph* sceneGraph, const SceneState* sceneState, PlatformContext& context);

        void frustumCull(const NodeCullParams& params, const vectorEASTL<SceneGraphNode*>& nodes, VisibleNodeList<>& nodesOut) const;
        void toVisibleNodes(const Camera* camera, const vectorEASTL<SceneGraphNode*>& nodes, VisibleNodeList<>& nodesOut) const;

        VisibleNodeList<>& getNodeCache(const RenderStage stage) noexcept { return _visibleNodes[to_U32(stage)]; }
        const VisibleNodeList<>& getNodeCache(const RenderStage stage) const noexcept { return _visibleNodes[to_U32(stage)]; }

    protected:
        void frustumCullNode(SceneGraphNode* currentNode, const NodeCullParams& params, U8 recursionLevel, VisibleNodeList<>& nodes) const;
        void addAllChildren(const SceneGraphNode* currentNode, const NodeCullParams& params,  VisibleNodeList<>& nodes) const;

    protected:
        std::array<VisibleNodeList<>, to_base(RenderStage::COUNT)> _visibleNodes;
};

}  // namespace Divide
#endif
