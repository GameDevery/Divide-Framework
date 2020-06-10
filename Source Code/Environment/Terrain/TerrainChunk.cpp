#include "stdafx.h"

#include "Headers/TerrainChunk.h"
#include "Headers/Terrain.h"
#include "Quadtree/Headers/QuadtreeNode.h"

#include "Managers/Headers/SceneManager.h"
#include "Geometry/Material/Headers/Material.h"
#include "Environment/Vegetation/Headers/Vegetation.h"

namespace Divide {

U32 TerrainChunk::_chunkID = 0;

TerrainChunk::TerrainChunk(GFXDevice& context,
                           Terrain* const parentTerrain,
                           QuadtreeNode& parentNode)
    : _context(context),
      _quadtreeNode(parentNode),
      _parentTerrain(parentTerrain),
      _ID(_chunkID++),
      _vegetation(nullptr),
      _xOffset(0),
      _yOffset(0),
      _sizeX(0),
      _sizeY(0)
{
}

void TerrainChunk::load(U8 depth, const vec2<U32>& pos, U32 _targetChunkDimension, const vec2<U32>& HMsize, BoundingBox& bbInOut) {
    _xOffset = to_F32(pos.x) - HMsize.x * 0.5f;
    _yOffset = to_F32(pos.y) - HMsize.y * 0.5f;
    _sizeX = _sizeY = to_F32(_targetChunkDimension);

    F32 tempMin = std::numeric_limits<F32>::max();
    F32 tempMax = std::numeric_limits<F32>::lowest();

    const U32 offset = to_U32(std::pow(2.0f, to_F32(0.0f)));
    const U32 div = to_U32(std::pow(2.0f, to_F32(depth)));

    const vec2<U32> heightmapDataSize = HMsize / (div);
    const U32 nHMWidth = heightmapDataSize.x;
    const U32 nHMHeight = heightmapDataSize.y;

    const vectorEASTL<VertexBuffer::Vertex>& verts = _parentTerrain->_physicsVerts;

    for (U16 j = 0; j < nHMHeight - 1; ++j) {
        const U32 jOffset = j * (offset)+pos.y;
        for (U16 i = 0; i < nHMWidth; ++i) {
            const U32 iOffset = i * (offset)+pos.x;
            U32 idx = iOffset + jOffset * HMsize.x;
            F32 height = verts[idx]._position.y;

            if (height > tempMax) {
                tempMax = height;
            }
            if (height < tempMin) {
                tempMin = height;
            }


            idx = iOffset + (jOffset + (offset)) * HMsize.x;
            height = verts[idx]._position.y;

            if (height > tempMax) {
                tempMax = height;
            }
            if (height < tempMin) {
                tempMin = height;
            }
        }
    }

    bbInOut.setMin(bbInOut.getMin().x, tempMin, bbInOut.getMin().z);
    bbInOut.setMax(bbInOut.getMax().x, tempMax, bbInOut.getMax().z);

    Attorney::TerrainChunk::registerTerrainChunk(*_parentTerrain, this);
}

void TerrainChunk::initializeVegetation(const VegetationDetails& vegDetails) {
    _vegetation.reset(new Vegetation(_context, *this, vegDetails));
}

const BoundingBox& TerrainChunk::bounds() const {
    return _quadtreeNode.getBoundingBox();
}

U8 TerrainChunk::LoD() const {
    return _quadtreeNode.LoD();
}

void TerrainChunk::drawBBox(RenderPackage& packageOut) {
    _quadtreeNode.drawBBox(packageOut);
}
};