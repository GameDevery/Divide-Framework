#include "stdafx.h"

#include "Headers/ParticleEmitter.h"


#include "Core/Headers/Configuration.h"
#include "Scenes/Headers/Scene.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/Headers/RenderPackage.h"
#include "Platform/Video/Headers/RenderStateBlock.h"
#include "Core/Headers/EngineTaskPool.h"
#include "Core/Headers/PlatformContext.h"
#include "Core/Resources/Headers/ResourceCache.h"
#include "Graphs/Headers/SceneGraphNode.h"
#include "Scenes/Headers/SceneState.h"
#include "Geometry/Material/Headers/Material.h"
#include "Rendering/Camera/Headers/Camera.h"
#include "ECS/Components/Headers/RenderingComponent.h"
#include "ECS/Components/Headers/BoundsComponent.h"
#include "ECS/Components/Headers/TransformComponent.h"
#include "Platform/Video/Buffers/VertexBuffer/GenericBuffer/Headers/GenericVertexData.h"

namespace Divide {
namespace {
    // 3 should always be enough for round-robin GPU updates to avoid stalls:
    // 1 in ram, 1 in driver and 1 in VRAM
    constexpr U32 g_particleBufferSizeFactor = 3;
    constexpr U32 g_particleGeometryBuffer = 0;
    constexpr U32 g_particlePositionBuffer = g_particleGeometryBuffer + 1;
    constexpr U32 g_particleColourBuffer = g_particlePositionBuffer* + 2;

    constexpr U64 g_updateInterval = Time::MillisecondsToMicroseconds(33);
}

ParticleEmitter::ParticleEmitter(GFXDevice& context, ResourceCache* parentCache, const size_t descriptorHash, const Str256& name)
    : SceneNode(parentCache,
                descriptorHash,
                name,
                ResourcePath(name),
                {},
                SceneNodeType::TYPE_PARTICLE_EMITTER,
                to_base(ComponentType::TRANSFORM) | to_base(ComponentType::BOUNDS) | to_base(ComponentType::RENDERING)),
      _context(context)
{
    for (U8 i = 0; i < s_MaxPlayerBuffers; ++i) {
        for (U8 j = 0; j < to_base(RenderStage::COUNT); ++j) {
            _particleGPUBuffers[i][j] = _context.newGVD(g_particleBufferSizeFactor);
            _particleGPUBuffers[i][j]->renderIndirect(true);
        }
    }

    _buffersDirty.fill(false);
}

ParticleEmitter::~ParticleEmitter()
{ 
    assert(_particles == nullptr);
}

GenericVertexData& ParticleEmitter::getDataBuffer(const RenderStage stage, const PlayerIndex idx) {
    return *_particleGPUBuffers[idx % s_MaxPlayerBuffers][to_U32(stage)];
}

bool ParticleEmitter::initData(const std::shared_ptr<ParticleData>& particleData) {
    // assert if double init!
    DIVIDE_ASSERT(particleData != nullptr, "ParticleEmitter::updateData error: Invalid particle data!");
    _particles = particleData;
    const vectorEASTL<vec3<F32>>& geometry = particleData->particleGeometryVertices();
    const vectorEASTL<U32>& indices = particleData->particleGeometryIndices();

    for (U8 i = 0; i < s_MaxPlayerBuffers; ++i) {
        for (U8 j = 0; j < to_base(RenderStage::COUNT); ++j) {
            GenericVertexData& buffer = getDataBuffer(static_cast<RenderStage>(j), i);

            buffer.create(3);

            GenericVertexData::SetBufferParams params = {};
            params._buffer = g_particleGeometryBuffer;
            params._bufferParams._elementCount = to_U32(geometry.size());
            params._bufferParams._elementSize = sizeof(vec3<F32>);
            params._bufferParams._updateFrequency = BufferUpdateFrequency::ONCE;
            params._bufferParams._updateUsage = BufferUpdateUsage::CPU_W_GPU_R;
            params._bufferParams._sync = false;
            params._bufferParams._initialData = { (Byte*)geometry.data(), geometry.size() * params._bufferParams._elementSize};
            params._useRingBuffer = false;

            buffer.setBuffer(params);

            if (!indices.empty()) {
                GenericVertexData::IndexBuffer idxBuff;
                idxBuff.smallIndices = false;
                idxBuff.count = to_U32(indices.size());
                idxBuff.data = (Byte*)indices.data();

                buffer.setIndexBuffer(idxBuff, BufferUpdateFrequency::ONCE);
            }

            AttributeDescriptor& desc = buffer.attribDescriptor(to_base(AttribLocation::POSITION));
            desc.set(g_particleGeometryBuffer, 3, GFXDataFormat::FLOAT_32);
        }
    }

    if (!updateData()) {
        return false;
    }

    // Generate a render state
    RenderStateBlock particleRenderState;
    particleRenderState.setCullMode(CullMode::NONE);
    particleRenderState.setZFunc(ComparisonFunction::EQUAL);
    _particleStateBlockHash = particleRenderState.getHash();

    particleRenderState.setZFunc(ComparisonFunction::LEQUAL);
    _particleStateBlockHashDepth = particleRenderState.getHash();

    bool useTexture = _particleTexture != nullptr;

    ShaderModuleDescriptor vertModule = {};
    vertModule._moduleType = ShaderType::VERTEX;
    vertModule._sourceFile = "particles.glsl";
    vertModule._variant = useTexture ? "WithTexture" : "NoTexture";

    ShaderModuleDescriptor fragModule = {};
    fragModule._moduleType = ShaderType::FRAGMENT;
    fragModule._sourceFile = "particles.glsl";
    fragModule._variant = useTexture ? "WithTexture" : "NoTexture";

    if (useTexture){
        fragModule._defines.emplace_back("HAS_TEXTURE", true);
    }

    ShaderProgramDescriptor shaderDescriptor = {};
    shaderDescriptor._modules.push_back(vertModule);
    shaderDescriptor._modules.push_back(fragModule);

    ResourceDescriptor particleShaderDescriptor(useTexture ? "particles_WithTexture" : "particles_NoTexture");
    particleShaderDescriptor.propertyDescriptor(shaderDescriptor);
    ShaderProgram_ptr particleShader = CreateResource<ShaderProgram>(_parentCache, particleShaderDescriptor);

    ResourceDescriptor particleShaderMainDescriptor(useTexture ? "particles_WithTexture_Main" : "particles_NoTexture_Main");
    ShaderProgramDescriptor shaderDescriptorMain = shaderDescriptor;
    shaderDescriptorMain._modules.back()._defines.emplace_back("MAIN_DISPLAY_PASS", true);
    particleShaderMainDescriptor.propertyDescriptor(shaderDescriptorMain);
    ShaderProgram_ptr particleShaderMain = CreateResource<ShaderProgram>(_parentCache, particleShaderMainDescriptor);

    shaderDescriptor._modules.back()._variant = "PrePass";
    shaderDescriptor._modules.back()._defines.emplace_back("PRE_PASS", true);
    ResourceDescriptor particleDepthShaderDescriptor(useTexture ? "particles_WithTexture" : "particles_NoTexture");
    particleDepthShaderDescriptor.propertyDescriptor(shaderDescriptor);
    ShaderProgram_ptr particleDepthShader = CreateResource<ShaderProgram>(_parentCache, particleDepthShaderDescriptor);

    shaderDescriptor = {};
    shaderDescriptor._modules.push_back(vertModule);
    shaderDescriptor._modules.push_back(fragModule);

    shaderDescriptor._modules.back()._variant = "Shadow.VSM";
    ResourceDescriptor particleDepthVSMShaderDescriptor(useTexture ? "particles_ShadowVSM_Texture" : "particles_ShadowVSM");
    particleDepthVSMShaderDescriptor.propertyDescriptor(shaderDescriptor);
    ShaderProgram_ptr particleShadowShader = CreateResource<ShaderProgram>(_parentCache, particleDepthVSMShaderDescriptor);

    shaderDescriptor._modules.back()._variant += ".ORTHO";
    shaderDescriptor._modules.back()._defines.emplace_back("ORTHO_PROJECTION", true);
    ResourceDescriptor particleDepthVSMShaderDescriptorOrtho(useTexture ? "particles_ShadowVSM_Ortho_Texture" : "particles_ShadowVSM_Ortho");
    particleDepthVSMShaderDescriptorOrtho.propertyDescriptor(shaderDescriptor);
    ShaderProgram_ptr particleShadowShaderOrtho = CreateResource<ShaderProgram>(_parentCache, particleDepthVSMShaderDescriptorOrtho);

    if (particleShader != nullptr) {
        Material_ptr mat = CreateResource<Material>(_parentCache, ResourceDescriptor(useTexture ? "Material_particles_Texture" : "Material_particles"));

        mat->setRenderStateBlock(_particleStateBlockHashDepth, RenderStage::COUNT, RenderPassType::PRE_PASS);
        mat->setRenderStateBlock(_particleStateBlockHash,      RenderStage::COUNT, RenderPassType::MAIN_PASS);
        mat->setRenderStateBlock(_particleStateBlockHash,      RenderStage::COUNT, RenderPassType::OIT_PASS);

        mat->setShaderProgram(particleDepthShader,       RenderStage::COUNT,  RenderPassType::PRE_PASS);
        mat->setShaderProgram(particleShader,            RenderStage::COUNT,  RenderPassType::MAIN_PASS);
        mat->setShaderProgram(particleShaderMain,        RenderStage::DISPLAY,  RenderPassType::MAIN_PASS);
        mat->setShaderProgram(particleShadowShader,      RenderStage::SHADOW, RenderPassType::COUNT);
        mat->setShaderProgram(particleShadowShaderOrtho, RenderStage::SHADOW, RenderPassType::COUNT, to_base(LightType::DIRECTIONAL));

        setMaterialTpl(mat);

        return true;
    }

    return false;
}

bool ParticleEmitter::updateData() {
    if (!_context.context().config().debug.renderFilter.particles) {
        return true;
    }

    constexpr U32 positionAttribLocation = 13;
    constexpr U32 colourAttribLocation = to_base(AttribLocation::COLOR);

    const U32 particleCount = _particles->totalCount();

    for (U8 i = 0; i < s_MaxPlayerBuffers; ++i) {
        for (U8 j = 0; j < to_base(RenderStage::COUNT); ++j) {
            GenericVertexData& buffer = getDataBuffer(static_cast<RenderStage>(j), i);

            GenericVertexData::SetBufferParams params = {};
            params._buffer = g_particlePositionBuffer;
            params._useRingBuffer = true;
            params._instanceDivisor = 1;

            params._bufferParams._elementCount = particleCount;
            params._bufferParams._elementSize = sizeof(vec4<F32>);
            params._bufferParams._updateFrequency = BufferUpdateFrequency::OFTEN;
            params._bufferParams._updateUsage = BufferUpdateUsage::CPU_W_GPU_R;
            params._bufferParams._sync = true;
            params._bufferParams._initialData = { nullptr, 0 };

            buffer.setBuffer(params);

            params._buffer = g_particleColourBuffer;
            params._bufferParams._elementCount = particleCount;
            params._bufferParams._elementSize = sizeof(UColour4);

            buffer.setBuffer(params);

            buffer.attribDescriptor(positionAttribLocation).set(g_particlePositionBuffer,
                                                                4,
                                                                GFXDataFormat::FLOAT_32,
                                                                false,
                                                                0);

            buffer.attribDescriptor(colourAttribLocation).set(g_particleColourBuffer,
                                                              4,
                                                              GFXDataFormat::UNSIGNED_BYTE,
                                                              true,
                                                              0);
        }
    }

    for (U32 i = 0; i < particleCount; ++i) {
        // Distance to camera (squared)
        _particles->_misc[i].w = -1.0f;
    }

    if (!_particles->_textureFileName.empty()) {
        TextureDescriptor textureDescriptor(TextureType::TEXTURE_2D);
        textureDescriptor.srgb(true);

        ResourceDescriptor texture(_particles->_textureFileName);
        texture.propertyDescriptor(textureDescriptor);

        _particleTexture = CreateResource<Texture>(_parentCache, texture);
    }

    return true;
}

bool ParticleEmitter::unload() {
    WAIT_FOR_CONDITION(getState() == ResourceState::RES_LOADED);
    
    _particles.reset();

    return SceneNode::unload();
}

void ParticleEmitter::buildDrawCommands(SceneGraphNode* sgn,
                                        const RenderStagePass& renderStagePass,
                                        const Camera& crtCamera,
                                        RenderPackage& pkgInOut) {
    if (_context.context().config().debug.renderFilter.particles) {
        if (_particleTexture) {
            SamplerDescriptor textureSampler = {};
            pkgInOut.get<GFX::BindDescriptorSetsCommand>(0)->_set._textureData.add(TextureEntry{ _particleTexture->data(), textureSampler.getHash(), TextureUsage::UNIT0 });
        }

        const Pipeline* pipeline = pkgInOut.get<GFX::BindPipelineCommand>(0)->_pipeline;
        PipelineDescriptor pipeDesc = pipeline->descriptor();

        pipeDesc._stateHash = renderStagePass.isDepthPass() ? _particleStateBlockHashDepth
            : _particleStateBlockHash;
        pkgInOut.get<GFX::BindPipelineCommand>(0)->_pipeline = _context.newPipeline(pipeDesc);

        U32 indexCount = to_U32(_particles->particleGeometryIndices().size());
        if (indexCount == 0) {
            indexCount = to_U32(_particles->particleGeometryVertices().size());
        }

        GenericDrawCommand cmd = {};
        cmd._primitiveType = _particles->particleGeometryType();
        cmd._cmd.indexCount = indexCount;

        pkgInOut.add(GFX::DrawCommand{ cmd });
    }

    SceneNode::buildDrawCommands(sgn, renderStagePass, crtCamera, pkgInOut);
}

void ParticleEmitter::prepareRender(SceneGraphNode* sgn,
                                    RenderingComponent& rComp,
                                    const RenderStagePass& renderStagePass,
                                    const Camera& camera,
                                    const bool refreshData) {

    if ( _enabled &&  getAliveParticleCount() > 0) {
        if (_context.context().config().debug.renderFilter.particles) {
            Wait(*_bufferUpdate, _context.context().taskPool(TaskPoolType::HIGH_PRIORITY));

            if (refreshData && _buffersDirty[to_U32(renderStagePass._stage)]) {
                GenericVertexData& buffer = getDataBuffer(renderStagePass._stage, 0);
                buffer.updateBuffer(g_particlePositionBuffer, 0u, to_U32(_particles->_renderingPositions.size()), _particles->_renderingPositions.data());
                buffer.updateBuffer(g_particleColourBuffer, 0u, to_U32(_particles->_renderingColours.size()), _particles->_renderingColours.data());
                buffer.incQueue();
                _buffersDirty[to_U32(renderStagePass._stage)] = false;
            }

            RenderPackage& pkg = sgn->get<RenderingComponent>()->getDrawPackage(renderStagePass);

            GenericDrawCommand& cmd = pkg.get<GFX::DrawCommand>(0)->_drawCommands[0];
            cmd._cmd.primCount = to_U32(_particles->_renderingPositions.size());
            cmd._sourceBuffer = getDataBuffer(renderStagePass._stage, 0).handle();
            cmd._bufferIndex = renderStagePass.baseIndex();

            if (renderStagePass._passType == RenderPassType::PRE_PASS) {
                const vec3<F32>& eyePos = camera.getEye();
                const U32 aliveCount = getAliveParticleCount();

                vectorEASTL<vec4<F32>>& misc = _particles->_misc;
                vectorEASTL<vec4<F32>>& pos = _particles->_position;


                ParallelForDescriptor descriptor = {};
                descriptor._iterCount = aliveCount;
                descriptor._partitionSize = 1000u;
                descriptor._cbk = [&eyePos, &misc, &pos](const Task*, const U32 start, const U32 end) {
                    for (U32 i = start; i < end; ++i) {
                        misc[i].w = pos[i].xyz.distanceSquared(eyePos);
                    }
                };

                parallel_for(_context.context(), descriptor);

                _bufferUpdate = CreateTask(
                    [this, &renderStagePass](const Task&) {
                    // invalidateCache means that the existing particle data is no longer partially sorted
                    _particles->sort();
                    _buffersDirty[to_U32(renderStagePass._stage)] = true;
                });

                Start(*_bufferUpdate, _context.context().taskPool(TaskPoolType::HIGH_PRIORITY));
            }
        }
    }

    SceneNode::prepareRender(sgn, rComp, renderStagePass, camera, refreshData);
}


/// Pre-process particles
void ParticleEmitter::sceneUpdate(const U64 deltaTimeUS,
                                  SceneGraphNode* sgn,
                                  SceneState& sceneState) {

    constexpr U32 s_particlesPerThread = 1024;

    if (_enabled) {
        U32 aliveCount = getAliveParticleCount();
        renderState().drawState(aliveCount > 0);

        TransformComponent* transform = sgn->get<TransformComponent>();

        const vec3<F32>& pos = transform->getPosition();
        const Quaternion<F32>& rot = transform->getOrientation();

        F32 averageEmitRate = 0;
        for (std::shared_ptr<ParticleSource>& source : _sources) {
            source->updateTransform(pos, rot);
            source->emit(g_updateInterval, _particles);
            averageEmitRate += source->emitRate();
        }
        averageEmitRate /= _sources.size();

        aliveCount = getAliveParticleCount();


        ParallelForDescriptor descriptor = {};
        descriptor._iterCount = aliveCount;
        descriptor._partitionSize = s_particlesPerThread;
        descriptor._cbk = [this](const Task*, const U32 start, const U32 end) {
            for (U32 i = start; i < end; ++i) {
                _particles->_position[i].w = _particles->_misc[i].z;
                _particles->_acceleration[i].set(0.0f);
            }
        };

        parallel_for(_context.context(), descriptor);

        ParticleData& data = *_particles;
        for (std::shared_ptr<ParticleUpdater>& up : _updaters) {
            up->update(g_updateInterval, data);
        }

        Wait(*_bbUpdate, _context.context().taskPool(TaskPoolType::HIGH_PRIORITY));

        _bbUpdate = CreateTask([this, aliveCount, averageEmitRate](const Task&)
        {
            BoundingBox aabb;
            for (U32 i = 0; i < aliveCount; i += to_U32(averageEmitRate) / 4) {
                aabb.add(_particles->_position[i]);
            }
            setBounds(aabb);
        });
        Start(*_bbUpdate, _context.context().taskPool(TaskPoolType::HIGH_PRIORITY));
    }

    SceneNode::sceneUpdate(deltaTimeUS, sgn, sceneState);
}

U32 ParticleEmitter::getAliveParticleCount() const {
    if (!_particles) {
        return 0u;
    }
    return _particles->aliveCount();
}

}
