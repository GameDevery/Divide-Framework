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
#ifndef _RENDERING_RENDER_PASS_RENDERPASS_H_
#define _RENDERING_RENDER_PASS_RENDERPASS_H_

#include "Platform/Video/Headers/RenderAPIWrapper.h"

namespace Divide {

namespace Time {
    class ProfileTimer;
};

struct Task;
struct RenderStagePass;

class SceneGraph;
class ShaderBuffer;
class SceneRenderState;
class RenderPassManager;
enum class RenderStage : U8;

// A RenderPass may contain multiple linked stages.
// Useful to avoid having multiple renderqueues per pass if 2 stages depend on one:
// E.g.: PRE_PASS + MAIN_PASS share the same renderqueue
class RenderPass : private NonCopyable {
   public:
       struct BufferData {
           ShaderBuffer* _nodeData = nullptr;
           ShaderBuffer* _cullCounter = nullptr;
           ShaderBuffer* _cmdBuffer = nullptr;
           U32* _lastCommandCount = nullptr;
           U32* _lastNodeCount = nullptr;

           U32 _elementOffset = 0;
       };
  public:
    // passStageFlags: the first stage specified will determine the data format used by the additional stages in the list
    explicit RenderPass(RenderPassManager& parent, GFXDevice& context, Str64 name, U8 sortKey, RenderStage passStageFlags, const vectorEASTL<U8>& dependencies, bool performanceCounters = false);
    ~RenderPass() = default;

    void render(const Task& parentTask, const SceneRenderState& renderState, GFX::CommandBuffer& bufferInOut) const;
    void postRender() const;

    U8 sortKey() const noexcept { return _sortKey; }
    const vectorEASTL<U8>& dependencies() const noexcept { return _dependencies; }
    U32 getLastTotalBinSize() const noexcept { return _lastNodeCount; }
    const Str64& name() const noexcept { return _name; }

    RenderStage stageFlag() const noexcept { return _stageFlag; }

    BufferData getBufferData(const RenderStagePass& stagePass) const;

    void initBufferData();

    static U8 DataBufferRingSize();

   private:
    GFXDevice & _context;
    RenderPassManager& _parent;

    mutable U32 _lastCmdCount = 0u;
    mutable U32 _lastNodeCount = 0u;

    U8 _sortKey = 0;
    vectorEASTL<U8> _dependencies;
    Str64 _name = "";
    RenderStage _stageFlag = RenderStage::COUNT;

    ShaderBuffer* _nodeData = nullptr;
	ShaderBuffer* _cullCounter = nullptr;
    ShaderBuffer* _cmdBuffer = nullptr;

	const bool _performanceCounters;
};

};  // namespace Divide

#endif
