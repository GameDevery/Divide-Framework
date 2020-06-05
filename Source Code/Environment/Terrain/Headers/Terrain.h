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
#ifndef _TERRAIN_H_
#define _TERRAIN_H_

#include "TerrainDescriptor.h"
#include "Geometry/Shapes/Headers/Object3D.h"
#include "Core/Resources/Headers/ResourceCache.h"
#include "Core/Math/BoundingVolumes/Headers/BoundingBox.h"
#include "Environment/Vegetation/Headers/Vegetation.h"
#include "Environment/Terrain/Quadtree/Headers/Quadtree.h"
#include "Rendering/Lighting/ShadowMapping/Headers/ShadowMap.h"
#include "Platform/Video/Buffers/VertexBuffer/Headers/VertexBuffer.h"

namespace Divide {

class TileRing;
class TerrainLoader;

template <typename T>
inline T TER_COORD(T x, T y, T width) noexcept {
    return x + (width * y);
}

enum class TerrainTextureChannel : U8 {
    TEXTURE_RED_CHANNEL = 0,
    TEXTURE_GREEN_CHANNEL = 1,
    TEXTURE_BLUE_CHANNEL = 2,
    TEXTURE_ALPHA_CHANNEL = 3,
    COUNT
};

class VertexBuffer;
class TerrainChunk;
class TerrainDescriptor;

FWD_DECLARE_MANAGED_CLASS(Quad3D);
FWD_DECLARE_MANAGED_CLASS(ShaderProgram);

namespace Attorney {
    class TerrainChunk;
    class TerrainLoader;
};

struct TessellationParams
{
    PROPERTY_RW(vec2<F32>, WorldScale);
    PROPERTY_RW(vec2<F32>, SnapGridSize);
    
    static constexpr U8 VTX_PER_TILE_EDGE = 9; // overlap => -2
    static constexpr U8 PATCHES_PER_TILE_EDGE = VTX_PER_TILE_EDGE - 1;
    static constexpr U16 QUAD_LIST_INDEX_COUNT = (VTX_PER_TILE_EDGE - 1) * (VTX_PER_TILE_EDGE - 1) * 4;

    void fromDescriptor(const std::shared_ptr<TerrainDescriptor>& descriptor);
};

class Terrain final : public Object3D {
    friend class Attorney::TerrainChunk;
    friend class Attorney::TerrainLoader;


   public:
       struct Vert {
           vec3<F32> _position;
           vec3<F32> _normal;
           vec3<F32> _tangent;
       };

   public:
    explicit Terrain(GFXDevice& context, ResourceCache* parentCache, size_t descriptorHash, const Str256& name);
    ~Terrain() = default;

    void toggleBoundingBoxes();

    [[nodiscard]] Vert      getVert(F32 x_clampf, F32 z_clampf, bool smooth) const;
    [[nodiscard]] Vert      getVertFromGlobal(F32 x, F32 z, bool smooth) const;
    [[nodiscard]] vec2<U16> getDimensions() const noexcept;
    [[nodiscard]] vec2<F32> getAltitudeRange() const noexcept;

    [[nodiscard]] inline const Quadtree& getQuadtree() const noexcept { return _terrainQuadtree; }

    void getVegetationStats(U32& maxGrassInstances, U32& maxTreeInstances) const;

    [[nodiscard]] inline const vectorEASTL<TerrainChunk*>& terrainChunks() const noexcept { return _terrainChunks; }
    [[nodiscard]] const std::shared_ptr<TerrainDescriptor>& descriptor() const noexcept { return _descriptor; }

    void saveToXML(boost::property_tree::ptree& pt) const final;
    void loadFromXML(const boost::property_tree::ptree& pt)  final;

   protected:
    [[nodiscard]] Vert getVert(F32 x_clampf, F32 z_clampf) const;
    [[nodiscard]] Vert getSmoothVert(F32 x_clampf, F32 z_clampf) const;

    void sceneUpdate(const U64 deltaTimeUS, SceneGraphNode* sgn, SceneState& sceneState) final;

    void onRefreshNodeData(const SceneGraphNode* sgn,
                           const RenderStagePass& renderStagePass,
                           const Camera& crtCamera,
                           bool refreshData,
                           GFX::CommandBuffer& bufferInOut) final;

    void postBuild();

    void buildDrawCommands(SceneGraphNode* sgn,
                           const RenderStagePass& renderStagePass,
                           const Camera& crtCamera,
                           RenderPackage& pkgInOut) final;

     bool prepareRender(SceneGraphNode* sgn,
                        RenderingComponent& rComp,
                        const RenderStagePass& renderStagePass,
                        const Camera& camera,
                        bool refreshData) final;

    void postLoad(SceneGraphNode* sgn);

    void onEditorChange(std::string_view field);

    [[nodiscard]] const char* getResourceTypeName() const noexcept override { return "Terrain"; }

    PROPERTY_R(TessellationParams, tessParams);

   public:
     vectorEASTL<VertexBuffer::Vertex> _physicsVerts;

   protected:

    F32 _drawDistance = 0.0f;
    VegetationDetails _vegDetails;

    Quadtree _terrainQuadtree;
    vectorEASTL<TerrainChunk*> _terrainChunks;
    GenericVertexData* _terrainBuffer = nullptr;
    vectorEASTL<eastl::unique_ptr<TileRing>> _tileRings;

    EditorDataState _editorDataDirtyState = EditorDataState::IDLE;
    bool _drawCommandsDirty = true;
    bool _initialSetupDone = false;

    SceneGraphNode* _vegetationGrassNode = nullptr;
    std::shared_ptr<TerrainDescriptor> _descriptor;

    //0 - normal, 1 - wireframe, 2 - normals
    std::array<ShaderProgram_ptr, to_base(TerrainDescriptor::WireframeMode::COUNT)> _terrainColourShader;
    std::array<ShaderProgram_ptr, to_base(TerrainDescriptor::WireframeMode::COUNT)> _terrainPrePassShader;
};

namespace Attorney {
class TerrainChunk {
private:
    [[nodiscard]] static const VegetationDetails& vegetationDetails(const Terrain& terrain) noexcept {
        return terrain._vegDetails;
    }

    static void registerTerrainChunk(Terrain& terrain, Divide::TerrainChunk* const chunk) {
        terrain._terrainChunks.push_back(chunk);
    }

    friend class Divide::TerrainChunk;
};

class TerrainLoader {
   private:
    [[nodiscard]] static VegetationDetails& vegetationDetails(Terrain& terrain) noexcept {
        return terrain._vegDetails;
    }

    [[nodiscard]] static BoundingBox& boundingBox(Terrain& terrain) noexcept {
        return terrain._boundingBox;
    }

    static void postBuild(Terrain& terrain) {
        return terrain.postBuild();
    }

    static void descriptor(Terrain& terrain, const std::shared_ptr<TerrainDescriptor>& descriptor) noexcept {
        terrain._descriptor = descriptor;
        terrain._tessParams.fromDescriptor(descriptor);
    }

    static void setShaderProgram(Terrain& terrain, const ShaderProgram_ptr& shader, bool prePass, TerrainDescriptor::WireframeMode mode) noexcept {
        if (!prePass) {
            terrain._terrainColourShader[to_base(mode)] = shader;
        } else {
            terrain._terrainPrePassShader[to_base(mode)] = shader;
        }
    }

    [[nodiscard]] static const ShaderProgram_ptr& getShaderProgram(Terrain& terrain, bool prePass, TerrainDescriptor::WireframeMode mode) noexcept {
        if (!prePass) {
            return terrain._terrainColourShader[to_base(mode)];
        }

        return terrain._terrainPrePassShader[to_base(mode)];
    }

    friend class Divide::TerrainLoader;
};

};  // namespace Attorney
};  // namespace Divide

#endif
