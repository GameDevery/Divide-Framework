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

#ifndef _TERRAIN_H_
#define _TERRAIN_H_

#include "Geometry/Shapes/Headers/Object3D.h"
#include "Core/Resources/Headers/ResourceCache.h"
#include "Core/Math/BoundingVolumes/Headers/BoundingBox.h"
#include "Environment/Vegetation/Headers/Vegetation.h"
#include "Environment/Terrain/Quadtree/Headers/Quadtree.h"
#include "Platform/Video/Buffers/VertexBuffer/Headers/VertexBuffer.h"

namespace Divide {

class TerrainLoader;

struct TerrainTextureLayer {
    TerrainTextureLayer() {
        _lastOffset = 0;
        _blendMap = nullptr;
        _tileMaps = nullptr;
        _normalMaps = nullptr;
    }

    ~TerrainTextureLayer();

    enum class TerrainTextureChannel : U32 {
        TEXTURE_RED_CHANNEL = 0,
        TEXTURE_GREEN_CHANNEL = 1,
        TEXTURE_BLUE_CHANNEL = 2,
        TEXTURE_ALPHA_CHANNEL = 3
    };

    inline void setBlendMap(Texture* const texture) { _blendMap = texture; }
    inline void setTileMaps(Texture* const texture) { _tileMaps = texture; }
    inline void setNormalMaps(Texture* const texture) { _normalMaps = texture; }
    inline void setDiffuseScale(TerrainTextureChannel textureChannel,
                                F32 scale) {
        _diffuseUVScale[to_uint(textureChannel)] = scale;
    }
    inline void setDetailScale(TerrainTextureChannel textureChannel,
                               F32 scale) {
        _detailUVScale[to_uint(textureChannel)] = scale;
    }

    inline const vec4<F32>& getDiffuseScales() const { return _diffuseUVScale; }
    inline const vec4<F32>& getDetailScales() const { return _detailUVScale; }

    Texture* const blendMap() const { return _blendMap; }
    Texture* const tileMaps() const { return _tileMaps; }
    Texture* const normalMaps() const { return _normalMaps; }

   private:
    U32 _lastOffset;
    vec4<F32> _diffuseUVScale;
    vec4<F32> _detailUVScale;
    Texture* _blendMap;
    Texture* _tileMaps;
    Texture* _normalMaps;
};

class Quad3D;
class Quadtree;
class Transform;
class VertexBuffer;
class ShaderProgram;
class SamplerDescriptor;
class TerrainDescriptor;

namespace Attorney {
    class TerrainChunk;
    class TerrainLoader;
};

class Terrain : public Object3D {
    friend class Attorney::TerrainChunk;
    friend class Attorney::TerrainLoader;

   public:
    Terrain();
    ~Terrain();

    bool unload();

    void postDrawBoundingBox(SceneGraphNode& sgn) const;
    inline void toggleBoundingBoxes() { _drawBBoxes = !_drawBBoxes; }

    vec3<F32> getPositionFromGlobal(F32 x, F32 z) const;
    vec3<F32> getPosition(F32 x_clampf, F32 z_clampf) const;
    vec3<F32> getNormal(F32 x_clampf, F32 z_clampf) const;
    vec3<F32> getTangent(F32 x_clampf, F32 z_clampf) const;
    const vec2<F32> getDimensions() {
        return vec2<F32>((F32)_terrainDimensions.x, (F32)_terrainDimensions.y);
    }

    void terrainSmooth(F32 k);
    void initializeVegetation(TerrainDescriptor* const terrain,
                              SceneGraphNode& terrainSGN);

    inline const Quadtree& getQuadtree() const { return _terrainQuadtree; }

    bool computeBoundingBox(SceneGraphNode& sgn);

   protected:
    void getDrawCommands(SceneGraphNode& sgn,
                         RenderStage renderStage,
                         SceneRenderState& sceneRenderState,
                         vectorImpl<GenericDrawCommand>& drawCommandsOut);

    void sceneUpdate(const U64 deltaTime, SceneGraphNode& sgn,
                     SceneState& sceneState);

    void buildQuadtree();
    void postLoad(SceneGraphNode& sgn);

    bool isInView(const SceneRenderState& sceneRenderState,
                  SceneGraphNode& sgn, const bool distanceCheck = true);

   protected:
    VegetationDetails _vegDetails;

    U8 _lightCount;
    vec2<U16> _terrainDimensions;
    U32 _chunkSize;
    Quadtree _terrainQuadtree;

    vec2<F32> _terrainScaleFactor;
    F32 _farPlane;
    bool _alphaTexturePresent;
    bool _drawBBoxes;
    bool _terrainInView;
    bool _planeInView;
    SceneGraphNode* _vegetationGrassNode;
    BoundingBox _boundingBox;
    Quad3D* _plane;
    F32 _underwaterDiffuseScale;
    vectorImpl<TerrainTextureLayer*> _terrainTextures;
    SamplerDescriptor* _albedoSampler;
    SamplerDescriptor* _normalSampler;

    vectorImpl<TerrainChunk*> _terrainChunks;
    /// Normal rendering state
    size_t _terrainRenderStateHash;
    /// Depth map rendering state
    size_t _terrainDepthRenderStateHash;
    /// Reflection rendering state
    size_t _terrainReflectionRenderStateHash;
};

namespace Attorney {
class TerrainChunk {
   private:
    static VegetationDetails& vegetationDetails(Terrain& terrain) {
        return terrain._vegDetails;
    }
    static void registerTerrainChunk(Terrain& terrain,
                                     Divide::TerrainChunk* const chunk) {
        terrain._terrainChunks.push_back(chunk);
    }
    friend class Divide::TerrainChunk;
};

class TerrainLoader {
   private:
    static void setUnderwaterDiffuseScale(Terrain& terrain, F32 diffuseScale) {
        terrain._underwaterDiffuseScale = diffuseScale;
    }
    /// Per terrain albedo sampler
    static const SamplerDescriptor& getAlbedoSampler(Terrain& terrain) {
        return *terrain._albedoSampler;
    }
    static const SamplerDescriptor& getNormalSampler(Terrain& terrain) {
        return *terrain._normalSampler;
    }
    static void addTextureLayer(Terrain& terrain,
                                TerrainTextureLayer* textureLayer) {
        terrain._terrainTextures.push_back(textureLayer);
    }
    static U32 textureLayerCount(Terrain& terrain) {
        return static_cast<U32>(terrain._terrainTextures.size());
    }
    static void setRenderStateHashes(Terrain& terrain, size_t normalStateHash,
                                     size_t reflectionStateHash,
                                     size_t depthStateHash) {
        terrain._terrainRenderStateHash = normalStateHash;
        terrain._terrainReflectionRenderStateHash = reflectionStateHash;
        terrain._terrainDepthRenderStateHash = depthStateHash;
    }
    static VegetationDetails& vegetationDetails(Terrain& terrain) {
        return terrain._vegDetails;
    }
    static void buildQuadtree(Terrain& terrain) { terrain.buildQuadtree(); }
    static BoundingBox& boundingBox(Terrain& terrain) {
        return terrain._boundingBox;
    }
    static F32& farPlane(Terrain& terrain) { return terrain._farPlane; }
    static Quad3D* plane(Terrain& terrain) { return terrain._plane; }
    static vec2<U16>& dimensions(Terrain& terrain) {
        return terrain._terrainDimensions;
    }
    static vec2<F32>& scaleFactor(Terrain& terrain) {
        return terrain._terrainScaleFactor;
    }
    static U32& chunkSize(Terrain& terrain) { return terrain._chunkSize; }

    friend class Divide::TerrainLoader;
};

};  // namespace Attorney
};  // namespace Divide

#endif
