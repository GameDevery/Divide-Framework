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

#ifndef _PARTICLE_EMITTER_H_
#define _PARTICLE_EMITTER_H_

#include "ParticleSource.h"
#include "ParticleUpdater.h"
#include "Graphs/Headers/SceneNode.h"
#include "Dynamics/Entities/Headers/Impostor.h"

/// original source code:
/// https://github.com/fenbf/particles/blob/public/particlesCode
namespace Divide {

class GenericVertexData;

FWD_DECLARE_MANAGED_CLASS(Texture);
/// A Particle emitter scene node. Nothing smarter to say, sorry :"> - Ionut
class ParticleEmitter : public SceneNode {
   public:
    explicit ParticleEmitter(GFXDevice& context, ResourceCache* parentCache, size_t descriptorHash, const Str128& name);
    ~ParticleEmitter();

    bool onRender(SceneGraphNode& sgn, RenderingComponent& rComp, const Camera& camera, RenderStagePass renderStagePass, bool refreshData) override;

    /// toggle the particle emitter on or off
    inline void enableEmitter(bool state) { _enabled = state; }

    inline void setDrawImpostor(const bool state) { _drawImpostor = state; }

    bool updateData(const std::shared_ptr<ParticleData>& particleData);
    bool initData(const std::shared_ptr<ParticleData>& particleData);

    /// SceneNode concrete implementations
    bool unload() noexcept override;
    
    inline void addUpdater(std::shared_ptr<ParticleUpdater> updater) {
        _updaters.push_back(updater);
    }

    inline void addSource(std::shared_ptr<ParticleSource> source) {
        _sources.push_back(source);
    }

    U32 getAliveParticleCount() const;

   protected:

    /// preprocess particles here
    void sceneUpdate(const U64 deltaTimeUS,
                     SceneGraphNode& sgn,
                     SceneState& sceneState) override;

    void buildDrawCommands(SceneGraphNode& sgn,
                           RenderStagePass renderStagePass,
                           RenderPackage& pkgInOut) override;

    void prepareForRender(RenderStagePass renderStagePass, const Camera& crtCamera);

    GenericVertexData& getDataBuffer(RenderStage stage, PlayerIndex idx);

    const char* getResourceTypeName() const override { return "ParticleEmitter"; }

   private:
    static constexpr U8 s_MaxPlayerBuffers = 4;

    GFXDevice& _context;
    std::shared_ptr<ParticleData> _particles;

    vectorSTD<std::shared_ptr<ParticleSource>> _sources;
    vectorSTD<std::shared_ptr<ParticleUpdater>> _updaters;

    /// create particles
    bool _enabled;
    /// draw the impostor?
    bool _drawImpostor;

    using BuffersPerStage = std::array<GenericVertexData*, to_base(RenderStage::COUNT)>;
    using BuffersPerPlayer = std::array<BuffersPerStage, s_MaxPlayerBuffers>;
    BuffersPerPlayer _particleGPUBuffers;
    std::array<bool, to_base(RenderStage::COUNT)> _buffersDirty;

    size_t _particleStateBlockHash;
    size_t _particleStateBlockHashDepth;

    Task* _bufferUpdate = nullptr;
    Task* _bbUpdate = nullptr;
    ShaderProgram_ptr _particleShader;
    ShaderProgram_ptr _particleDepthShader[2];
    Texture_ptr _particleTexture;
};

TYPEDEF_SMART_POINTERS_FOR_TYPE(ParticleEmitter);

};  // namespace Divide

#endif