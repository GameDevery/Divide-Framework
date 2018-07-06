#include "Headers/ParticleEmitter.h"

#include "Core/Headers/Application.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Core/Resources/Headers/ResourceCache.h"
#include "Core/Headers/ParamHandler.h"
#include "Graphs/Headers/SceneGraphNode.h"
#include "Core/Math/Headers/Transform.h"
#include "Scenes/Headers/SceneState.h"
#include "Geometry/Material/Headers/Material.h"

namespace Divide {

ParticleEmitter::ParticleEmitter()
    : SceneNode(SceneNodeType::TYPE_PARTICLE_EMITTER),
      _drawImpostor(false),
      _updateParticleEmitterBB(true),
      _particleStateBlockHash(0),
      _enabled(false),
      _uploaded(false),
      _particleTexture(nullptr),
      _particleShader(nullptr),
      _particleGPUBuffer(nullptr),
      _particleDepthShader(nullptr) {
    _drawCommand = GenericDrawCommand(PrimitiveType::TRIANGLE_STRIP, 0, 4, 1);
    _readOffset = 0;
    _writeOffset = 2;
}

ParticleEmitter::~ParticleEmitter() { 
    unload(); 
}

bool ParticleEmitter::initData(std::shared_ptr<ParticleData> particleData) {
    // assert if double init!
    DIVIDE_ASSERT(_particleGPUBuffer == nullptr,
                  "ParticleEmitter::initData error: Double initData detected!");

    _particleGPUBuffer = GFX_DEVICE.newGVD(/*true*/false);
    _particleGPUBuffer->create(3);

    // Not using Quad3D to improve performance
    static F32 particleQuad[] = {
        -0.5f, -0.5f, 0.0f, 
         0.5f, -0.5f, 0.0f,
        -0.5f,  0.5f, 0.0f,
         0.5f,  0.5f, 0.0f,
    };

    _particleGPUBuffer->setBuffer(0, 12, sizeof(F32), 1, particleQuad, false,
                                  false);
    _particleGPUBuffer->getDrawAttribDescriptor(
                            to_uint(
                                AttribLocation::VERTEX_POSITION))
        .set(0, 0, 3, false, 0, 0, GFXDataFormat::FLOAT_32);

    updateData(particleData);

    // Generate a render state
    RenderStateBlock particleRenderState;
    particleRenderState.setCullMode(CullMode::NONE);
    particleRenderState.setBlend(true, BlendProperty::SRC_ALPHA,
                                  BlendProperty::INV_SRC_ALPHA);
    particleRenderState.setZReadWrite(true, false);
    particleRenderState.setZFunc(ComparisonFunction::LEQUAL);
    _particleStateBlockHash = particleRenderState.getHash();

    ResourceDescriptor particleShaderDescriptor("particles");
    _particleShader = CreateResource<ShaderProgram>(particleShaderDescriptor);
    _particleShader->Uniform("hasTexture", _particleTexture != nullptr);
    REGISTER_TRACKED_DEPENDENCY(_particleShader);

    ResourceDescriptor particleDepthShaderDescriptor("particles.Depth");
    _particleDepthShader = CreateResource<ShaderProgram>(particleDepthShaderDescriptor);
    REGISTER_TRACKED_DEPENDENCY(_particleDepthShader);

    _impostor =
        CreateResource<ImpostorBox>(ResourceDescriptor(_name + "_impostor"));
    _impostor->renderState().setDrawState(false);
    _impostor->getMaterialTpl()->setDiffuse(vec4<F32>(0.0f, 0.0f, 1.0f, 1.0f));
    _impostor->getMaterialTpl()->setAmbient(vec4<F32>(0.0f, 0.0f, 1.0f, 1.0f));

    //_renderState.addToDrawExclusionMask(RenderStage::SHADOW);

    return (_particleShader != nullptr);
}

bool ParticleEmitter::updateData(std::shared_ptr<ParticleData> particleData) {
    DIVIDE_ASSERT(particleData.get() != nullptr,
                  "ParticleEmitter::updateData error: Invalid particle data!");

    _particles = particleData;

    U32 particleCount = _particles->totalCount();

    _particleGPUBuffer->setBuffer(1, particleCount * 3, 4 * sizeof(F32), 1,
                                  NULL, true, true, /*true*/ false);
    _particleGPUBuffer->setBuffer(2, particleCount * 3, 4 * sizeof(U8), 1, NULL,
                                  true, true, /*true*/ false);

    _particleGPUBuffer->getDrawAttribDescriptor(13)
        .set(1, 1, 4, false, 4 * sizeof(F32), 0, GFXDataFormat::FLOAT_32);
    _particleGPUBuffer->getDrawAttribDescriptor(
                            to_uint(AttribLocation::VERTEX_COLOR))
        .set(2, 1, 4, true, 4 * sizeof(U8), 0, GFXDataFormat::UNSIGNED_BYTE);

    for (U32 i = 0; i < particleCount; ++i) {
        // Distance to camera (squared)
        _particles->_misc[i].w = -1.0f;
        _particles->_alive[i] = false;
    }

    if (_particleTexture) {
        UNREGISTER_TRACKED_DEPENDENCY(_particleTexture);
        RemoveResource(_particleTexture);
    }

    if (!_particles->_textureFileName.empty()) {
        SamplerDescriptor textureSampler;
        textureSampler.toggleSRGBColorSpace(true);

        ResourceDescriptor texture(_particles->_textureFileName);
        texture.setResourceLocation(
            ParamHandler::getInstance().getParam<stringImpl>("assetsLocation") +
            "/" +
            ParamHandler::getInstance().getParam<stringImpl>(
                "defaultTextureLocation") +
            "/" + _particles->_textureFileName);
        texture.setPropertyDescriptor<SamplerDescriptor>(textureSampler);

        _particleTexture = CreateResource<Texture>(texture);

        REGISTER_TRACKED_DEPENDENCY(_particleTexture);
    }

    return true;
}

bool ParticleEmitter::unload() {
    if (getState() != ResourceState::RES_LOADED &&
        getState() != ResourceState::RES_LOADING) {
        return true;
    }
    if (_particleTexture) {
        UNREGISTER_TRACKED_DEPENDENCY(_particleTexture);
    }
    UNREGISTER_TRACKED_DEPENDENCY(_particleShader);
    UNREGISTER_TRACKED_DEPENDENCY(_particleDepthShader);
    RemoveResource(_particleTexture);
    RemoveResource(_particleShader);
    RemoveResource(_particleDepthShader);

    MemoryManager::DELETE(_particleGPUBuffer);

    _particles.reset();

    return SceneNode::unload();
}

void ParticleEmitter::postLoad(SceneGraphNode& sgn) {
    sgn.addNode(*_impostor)->setActive(false);

    
    Framebuffer* depthBuffer = GFX_DEVICE.getRenderTarget(GFXDevice::RenderTarget::DEPTH);
    Texture* depthTexture = depthBuffer->GetAttachment(TextureDescriptor::AttachmentType::Depth);
    TextureData depthBufferData = depthTexture->getData();
    depthBufferData.setHandleLow(to_uint(ShaderProgram::TextureUsage::UNIT1));
    sgn.getComponent<RenderingComponent>()->registerTextureDependency(depthBufferData);

    if (_particleTexture && _particleTexture->flushTextureState()) {
        TextureData particleTextureData = _particleTexture->getData();
        particleTextureData.setHandleLow(to_uint(ShaderProgram::TextureUsage::UNIT0));
        sgn.getComponent<RenderingComponent>()->registerTextureDependency(particleTextureData);
    }

    SceneNode::postLoad(sgn);
}

bool ParticleEmitter::computeBoundingBox(SceneGraphNode& sgn) {
    if (!_enabled) {
        return false;
    }
    DIVIDE_ASSERT(_particles.get() != nullptr,
                  "ParticleEmitter::computeBoundingBox error: BoundingBox "
                  "calculation requested without valid particle data "
                  "available!");
    _updateParticleEmitterBB = true;
    sgn.getBoundingBox().set(vec3<F32>(-5), vec3<F32>(5));
    return SceneNode::computeBoundingBox(sgn);
}

void ParticleEmitter::onCameraUpdate(SceneGraphNode& sgn, Camera& camera) {
    const mat4<F32>& viewMatrixCache =
        GFX_DEVICE.getMatrix(MATRIX_MODE::VIEW);
    _particleShader->Uniform(
        "CameraRight_worldspace",
        vec3<F32>(viewMatrixCache.m[0][0],
                  viewMatrixCache.m[1][0],
                  viewMatrixCache.m[2][0]));
    _particleShader->Uniform(
        "CameraUp_worldspace",
        vec3<F32>(viewMatrixCache.m[0][1],
                  viewMatrixCache.m[1][1],
                  viewMatrixCache.m[2][1]));
    _particleDepthShader->Uniform(
        "CameraRight_worldspace",
        vec3<F32>(viewMatrixCache.m[0][0],
                  viewMatrixCache.m[1][0],
                  viewMatrixCache.m[2][0]));
    _particleDepthShader->Uniform(
        "CameraUp_worldspace",
        vec3<F32>(viewMatrixCache.m[0][1],
                  viewMatrixCache.m[1][1],
                  viewMatrixCache.m[2][1]));
}

void ParticleEmitter::getDrawCommands(
                            SceneGraphNode& sgn,
                            RenderStage renderStage,
                            const SceneRenderState& sceneRenderState,
                            vectorImpl<GenericDrawCommand>& drawCommandsOut) {

    U32 particleCount = getAliveParticleCount();
    if (!_enabled || particleCount == 0) {
        return;
    }

    RenderingComponent* renderable = sgn.getComponent<RenderingComponent>();
    _drawCommand.renderGeometry(renderable->renderGeometry());
    _drawCommand.renderWireframe(renderable->renderWireframe());
    _drawCommand.stateHash(_particleStateBlockHash);
    _drawCommand.cmd().primCount = particleCount;

    _drawCommand.shaderProgram(renderStage == RenderStage::DISPLAY
                                   ? _particleShader
                                   : _particleDepthShader);
    _drawCommand.sourceBuffer(_particleGPUBuffer);
    drawCommandsOut.push_back(_drawCommand);
}

void ParticleEmitter::uploadToGPU() {
    if (_uploaded || getAliveParticleCount() == 0) {
        return;
    }

    U32 writeOffset = _writeOffset * to_uint(_particles->totalCount());
    U32 readOffset = _readOffset * to_uint(_particles->totalCount());

    _particleGPUBuffer->updateBuffer(1, _particles->aliveCount(), writeOffset,
                                     _particles->_renderingPositions.data());
    _particleGPUBuffer->updateBuffer(2, _particles->aliveCount(), writeOffset,
                                     _particles->_color.data());

    _particleGPUBuffer->getDrawAttribDescriptor(13)
        .set(1, 1, 4, false, 4 * sizeof(F32), readOffset, GFXDataFormat::FLOAT_32);
    _particleGPUBuffer->getDrawAttribDescriptor(
                            to_uint(AttribLocation::VERTEX_COLOR))
        .set(2, 1, 4, true, 4 * sizeof(U8), readOffset, GFXDataFormat::UNSIGNED_BYTE);

    _writeOffset = (_writeOffset + 1) % 3;
    _readOffset = (_readOffset + 1) % 3;

    _uploaded = true;
}

/// The onDraw call will emit particles
bool ParticleEmitter::onDraw(SceneGraphNode& sgn,
                             RenderStage currentStage) {
    if (_enabled) {
        _particles->sort();
        uploadToGPU();
        return true;
    }

    return false;
}

/// Pre-process particles
void ParticleEmitter::sceneUpdate(const U64 deltaTime, SceneGraphNode& sgn,
                                  SceneState& sceneState) {
    if (!_enabled) {
        return;
    }

    
    renderState().setDrawState(getAliveParticleCount() != 0);

    PhysicsComponent* const transform = sgn.getComponent<PhysicsComponent>();
    const vec3<F32>& eyePos =
        sceneState.renderState().getCameraConst().getEye();

    if (_updateParticleEmitterBB) {
        sgn.updateBoundingBoxTransform(transform->getWorldMatrix());
        _updateParticleEmitterBB = false;
    }

    for (std::shared_ptr<ParticleSource>& source : _sources) {
        source->emit(deltaTime, _particles.get());
    }

    U32 count = _particles->totalCount();
    U8 lodLevel = sgn.getComponent<RenderingComponent>()->lodLevel();
    for (U32 i = 0; i < count; ++i) {
        _particles->_misc[i].w =  _particles->_position[i].xyz().distanceSquared(eyePos);
        _particles->_position[i].w = 1.0f;
        _particles->_acceleration[i].set(0.0f);
        _particles->lodLevel(lodLevel);
    }

    for (std::shared_ptr<ParticleUpdater>& up : _updaters) {
        up->update(deltaTime, _particles.get());
    }

    // const vec3<F32>& origin = transform->getPosition();
    // const Quaternion<F32>& orientation = transform->getOrientation();

    _uploaded = false;

    SceneNode::sceneUpdate(deltaTime, sgn, sceneState);
}

U32 ParticleEmitter::getAliveParticleCount() const {
    if (!_particles.get()) {
        return 0;
    }
    return _particles->aliveCount();
}
};
