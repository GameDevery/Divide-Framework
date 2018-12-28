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

#ifndef _TERRAIN_CHUNK_H
#define _TERRAIN_CHUNK_H

#include "config.h"

#include "Platform/Video/Buffers/VertexBuffer/Headers/VertexBuffer.h"

namespace Divide {

class Mesh;
class Terrain;
class BoundingBox;
class QuadtreeNode;
class ShaderProgram;
class SceneGraphNode;
class SceneRenderState;
struct FileData;

FWD_DECLARE_MANAGED_CLASS(Vegetation);

namespace Attorney {
    class TerrainChunkTerrain;
};

class TerrainChunk {
    static U32 _chunkID;
    friend class Attorney::TerrainChunkTerrain;

   public:
    TerrainChunk(GFXDevice& context, Terrain* const parentTerrain, const QuadtreeNode& parentNode);
    ~TerrainChunk();

    void load(U8 depth, const vec2<U32>& pos, U32 targetChunkDimension, const vec2<U32>& HMsize);

    inline U32 ID() const { return _ID; }
    inline F32 getMinHeight() const { return _heightBounds.x; }
    inline F32 getMaxHeight() const { return _heightBounds.y; }

    inline vec4<F32> getOffsetAndSize() const {
        return vec4<F32>(_xOffset, _yOffset, _sizeX, _sizeY);
    }

    inline const Terrain& parent() const { return *_parentTerrain; }
    inline const QuadtreeNode& quadtreeNode() const { return _quadtreeNode; };

    const BoundingBox& bounds() const;

    bool isInView() const;

   protected:
    const Vegetation_ptr& getVegetation() const { return _vegetation; }

   private:
    GFXDevice& _context;
    const QuadtreeNode& _quadtreeNode;

    U32 _ID;
    F32 _xOffset;
    F32 _yOffset;
    F32 _sizeX;
    F32 _sizeY;
    vec2<F32> _heightBounds;  //< 0 = minHeight, 1 = maxHeight
    Terrain* _parentTerrain;
    Vegetation_ptr _vegetation;
};

namespace Attorney {
class TerrainChunkTerrain {
   private:
    static const Vegetation_ptr& getVegetation(Divide::TerrainChunk& chunk) {
        return chunk.getVegetation();
    }
    friend class Divide::Terrain;
};
};  // namespace Attorney
};  // namespace Divide

#endif
