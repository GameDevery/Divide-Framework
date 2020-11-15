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
#ifndef _QUAD_TREE
#define _QUAD_TREE

#include "Platform/Video/Headers/RenderAPIWrapper.h"

namespace Divide {

class Terrain;
class SceneState;
class BoundingBox;
class IMPrimitive;
class VertexBuffer;
class QuadtreeNode;
class RenderPackage;
class ShaderProgram;
class SceneGraphNode;
class SceneRenderState;

class Quadtree {
  public:
    Quadtree(GFXDevice& context);
    ~Quadtree();

    void build(BoundingBox& terrainBBox,
               const vec2<U16>& HMSize,
               U32 targetChunkDimension,
               Terrain* terrain);

    [[nodiscard]] const BoundingBox& computeBoundingBox() const;

    [[nodiscard]] U32 getChunkCount() const noexcept { return _chunkCount; }

    void drawBBox(RenderPackage& packageOut) const;
    void toggleBoundingBoxes();

    [[nodiscard]] QuadtreeNode* findLeaf(const vec2<F32>& pos) const;

    POINTER_R(Pipeline, bbPipeline, nullptr);

   private:
    eastl::unique_ptr<QuadtreeNode> _root = nullptr;
    GFXDevice&    _context;
    VertexBuffer* _parentVB = nullptr;
    IMPrimitive*  _bbPrimitive = nullptr;
    U32 _chunkCount = 0u;
    bool _drawBBoxes = false;
};

}  // namespace Divide

#endif
