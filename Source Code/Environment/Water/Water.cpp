#include "Headers/Water.h"

#include "Core/Headers/ParamHandler.h"
#include "Core/Math/Headers/Transform.h"
#include "Managers/Headers/SceneManager.h"
#include "Platform/Video/Headers/RenderStateBlock.h"
#include "Geometry/Material/Headers/Material.h"

namespace Divide {

WaterPlane::WaterPlane()
    : SceneNode(SceneNodeType::TYPE_WATER),
      Reflector(ReflectorType::TYPE_WATER_SURFACE, vec2<U16>(1024, 1024)),
      _plane(nullptr),
      _waterLevel(0),
      _waterDepth(0),
      _refractionPlaneID(ClipPlaneIndex::COUNT),
      _reflectionPlaneID(ClipPlaneIndex::COUNT),
      _reflectionRendering(false),
      _refractionRendering(false),
      _refractionTexture(nullptr),
      _dirty(true),
      _paramsDirty(true),
      _cameraUnderWater(false),
      _cameraMgr(Application::getInstance().getKernel().getCameraMgr()) {
    // Set water plane to be single-sided
    P32 quadMask;
    quadMask.i = 0;
    quadMask.b.b0 = true;

    setParams(50.0f, vec2<F32>(10.0f, 10.0f), vec2<F32>(0.1f, 0.1f), 0.34f);

    ResourceDescriptor waterPlane("waterPlane");
    waterPlane.setFlag(true);  // No default material
    waterPlane.setBoolMask(quadMask);
    _plane = CreateResource<Quad3D>(waterPlane);
    _farPlane = 2.0f *
                GET_ACTIVE_SCENE()
                    ->state()
                    .getRenderState()
                    .getCameraConst()
                    .getZPlanes()
                    .y;
    _plane->setCorner(Quad3D::CornerLocation::TOP_LEFT,
                      vec3<F32>(-_farPlane * 1.5f, 0, -_farPlane * 1.5f));
    _plane->setCorner(Quad3D::CornerLocation::TOP_RIGHT,
                      vec3<F32>(_farPlane * 1.5f, 0, -_farPlane * 1.5f));
    _plane->setCorner(Quad3D::CornerLocation::BOTTOM_LEFT,
                      vec3<F32>(-_farPlane * 1.5f, 0, _farPlane * 1.5f));
    _plane->setCorner(Quad3D::CornerLocation::BOTTOM_RIGHT,
                      vec3<F32>(_farPlane * 1.5f, 0, _farPlane * 1.5f));
    _plane->setNormal(Quad3D::CornerLocation::CORNER_ALL, WORLD_Y_AXIS);
    _plane->renderState().setDrawState(false);
    // The water doesn't cast shadows, doesn't need ambient occlusion and
    // doesn't have real "depth"
    renderState().addToDrawExclusionMask(RenderStage::SHADOW_STAGE);
    Console::printfn(Locale::get("REFRACTION_INIT_FB"), _resolution.x,
                     _resolution.y);
    SamplerDescriptor refractionSampler;
    refractionSampler.setWrapMode(TextureWrap::TEXTURE_CLAMP_TO_EDGE);
    refractionSampler.toggleMipMaps(false);
    // Less precision for reflections
    TextureDescriptor refractionDescriptor(TextureType::TEXTURE_2D,
                                           GFXImageFormat::RGBA8,
                                           GFXDataFormat::UNSIGNED_BYTE);  
    refractionDescriptor.setSampler(refractionSampler);

    _refractionTexture = GFX_DEVICE.newFB();
    _refractionTexture->AddAttachment(refractionDescriptor,
                                      TextureDescriptor::AttachmentType::Color0);
    _refractionTexture->toggleDepthBuffer(true);
    _refractionTexture->Create(_resolution.x, _resolution.y);
}

void WaterPlane::postLoad(SceneGraphNode& sgn) {
    sgn.addNode(_plane);
    SceneNode::postLoad(sgn);
}

bool WaterPlane::computeBoundingBox(SceneGraphNode& sgn) {
    BoundingBox& bb = sgn.getBoundingBox();

    if (bb.isComputed()) {
        return true;
    }
    SceneGraphNode* planeSGN = sgn.getChildren()[0];
    _waterLevel = GET_ACTIVE_SCENE()->state().getWaterLevel();
    _waterDepth = GET_ACTIVE_SCENE()->state().getWaterDepth();
    planeSGN->getComponent<PhysicsComponent>()->setPositionY(_waterLevel);
    bb.set(vec3<F32>(-_farPlane, _waterLevel - _waterDepth, -_farPlane),
           vec3<F32>(_farPlane, _waterLevel, _farPlane));
    planeSGN->getBoundingBox().Add(bb);
    Console::printfn(Locale::get("WATER_CREATE_DETAILS_1"), bb.getMax().y);
    Console::printfn(Locale::get("WATER_CREATE_DETAILS_2"), bb.getMin().y);
    _dirty = true;
    return SceneNode::computeBoundingBox(sgn);
}

bool WaterPlane::unload() {
    bool state = false;
    state = SceneNode::unload();
    return state;
}

void WaterPlane::setParams(F32 shininess, const vec2<F32>& noiseTile,
                           const vec2<F32>& noiseFactor, F32 transparency) {
    _shininess = shininess;
    _noiseTile = noiseTile;
    _noiseFactor = noiseFactor;
    _transparency = transparency;
    _paramsDirty = true;
}

void WaterPlane::sceneUpdate(const U64 deltaTime, SceneGraphNode& sgn,
                             SceneState& sceneState) {
    _cameraUnderWater =
        isPointUnderWater(sceneState.getRenderState().getCamera().getEye());
    if (_dirty) {
        sgn.getBoundingSphere().fromBoundingBox(sgn.getBoundingBoxConst());
        _dirty = false;
    }
    if (_paramsDirty) {
        RenderingComponent* rComp = sgn.getComponent<RenderingComponent>();
        ShaderProgram* shader = rComp->getMaterialInstance()
                                    ->getShaderInfo(RenderStage::FINAL_STAGE)
                                    .getProgram();
        shader->Uniform("_waterShininess", _shininess);
        shader->Uniform("_noiseFactor", _noiseFactor);
        shader->Uniform("_noiseTile", _noiseTile);
        shader->Uniform("_transparencyBias", _transparency);
        _paramsDirty = false;
    }
}

bool WaterPlane::onDraw(SceneGraphNode& sgn, const RenderStage& currentStage) {
    const Quaternion<F32>& orientation =
        sgn.getComponent<PhysicsComponent>()->getOrientation();
    if (!_orientation.compare(orientation)) {
        _orientation.set(orientation);
        updatePlaneEquation();
    }

    if (!_plane->onDraw(currentStage)) {
        return false;
    }

    if (!GFX_DEVICE.isCurrentRenderStage(RenderStage::DEPTH_STAGE)) {
        TextureDescriptor::AttachmentType att =
            TextureDescriptor::AttachmentType::Color0;
        RenderingComponent* renderable = sgn.getComponent<RenderingComponent>();
        renderable->makeTextureResident(
            *(_reflectedTexture->GetAttachment(att)), 1);
        if (!_cameraUnderWater) {
            renderable->makeTextureResident(
                *(_refractionTexture->GetAttachment(att)), 2);
        }
    }

    return true;
}

void WaterPlane::postDraw(SceneGraphNode& sgn,
                          const RenderStage& currentStage) {}

void WaterPlane::getDrawCommands(
    SceneGraphNode& sgn, const RenderStage& currentRenderStage,
    SceneRenderState& sceneRenderState,
    vectorImpl<GenericDrawCommand>& drawCommandsOut) {
    bool depthPass = GFX_DEVICE.isCurrentRenderStage(RenderStage::DEPTH_STAGE);
    RenderingComponent* const renderable =
        sgn.getComponent<RenderingComponent>();
    assert(renderable != nullptr);

    ShaderProgram* drawShader =
        renderable->getDrawShader(depthPass ? RenderStage::Z_PRE_PASS_STAGE : RenderStage::FINAL_STAGE);
    drawShader->Uniform("underwater", _cameraUnderWater);
    GenericDrawCommand cmd(PrimitiveType::TRIANGLE_STRIP, 0, 0);
    cmd.renderWireframe(renderable->renderWireframe());
    cmd.stateHash(
        renderable->getMaterialInstance()->getRenderStateBlock(RenderStage::FINAL_STAGE));
    cmd.shaderProgram(drawShader);
    cmd.sourceBuffer(_plane->getGeometryVB());
    drawCommandsOut.push_back(cmd);
}


bool WaterPlane::getDrawState(const RenderStage& currentStage) {
    // Wait for the Reflector to update
    if (!_createdFB) {
        return false;
    }

    // Make sure we are not drawing our self unless this is desired
    if ((currentStage == RenderStage::REFLECTION_STAGE || _reflectionRendering ||
         _refractionRendering) &&
        !_updateSelf) {
        return false;
    }

    // Else, process normal exclusion
    return SceneNode::getDrawState(currentStage);
}

/// update water refraction
void WaterPlane::updateRefraction() {
    if (_cameraUnderWater) {
        return;
    }
    // Early out check for render callback
    if (!_renderCallback) {
        return;
    }
    _refractionRendering = true;
    // If we are above water, process the plane's reflection. If we are below,
    // we render the scene normally
    RenderStage prevRenderStage = GFX_DEVICE.setRenderStage(RenderStage::FINAL_STAGE);
    GFX_DEVICE.toggleClipPlane(_refractionPlaneID, true);
    _cameraMgr.getActiveCamera()->renderLookAt();
    // bind the refractive texture
    _refractionTexture->Begin(Framebuffer::defaultPolicy());
    // render to the reflective texture
    _refractionCallback();
    _refractionTexture->End();

    GFX_DEVICE.toggleClipPlane(_refractionPlaneID, false);
    GFX_DEVICE.setRenderStage(prevRenderStage);

    _refractionRendering = false;
}

/// Update water reflections
void WaterPlane::updateReflection() {
    // Early out check for render callback
    if (_renderCallback) {
        // ToDo: this will cause problems later with multiple reflectors. Fix
        // it! -Ionut
        _reflectionRendering = true;

        RenderStage prevRenderStage = GFX_DEVICE.setRenderStage(
            _cameraUnderWater ? RenderStage::FINAL_STAGE : RenderStage::REFLECTION_STAGE);
        GFX_DEVICE.toggleClipPlane(_reflectionPlaneID, true);

        _cameraUnderWater ? _cameraMgr.getActiveCamera()->renderLookAt()
                          : _cameraMgr.getActiveCamera()->renderLookAtReflected(
                                getReflectionPlane());

        _reflectedTexture->Begin(Framebuffer::defaultPolicy());
        _renderCallback();  //< render to the reflective texture
        _reflectedTexture->End();

        GFX_DEVICE.toggleClipPlane(_reflectionPlaneID, false);
        GFX_DEVICE.setRenderStage(prevRenderStage);

        _reflectionRendering = false;
    }
    updateRefraction();
}

void WaterPlane::updatePlaneEquation() {
    vec3<F32> reflectionNormal(_orientation * WORLD_Y_AXIS);
    reflectionNormal.normalize();
    vec3<F32> refractionNormal(_orientation * WORLD_Y_NEG_AXIS);
    refractionNormal.normalize();
    _reflectionPlane.set(reflectionNormal, -_waterLevel);
    _reflectionPlane.active(false);
    _refractionPlane.set(refractionNormal, -_waterLevel);
    _refractionPlane.active(false);
    _reflectionPlaneID = ClipPlaneIndex::CLIP_PLANE_0;
    _refractionPlaneID = ClipPlaneIndex::CLIP_PLANE_1;

    GFX_DEVICE.setClipPlane(_reflectionPlaneID, _reflectionPlane);
    GFX_DEVICE.setClipPlane(_refractionPlaneID, _refractionPlane);

    _dirty = true;
}

void WaterPlane::previewReflection() {
#ifdef _DEBUG
    if (_previewReflection) {
        F32 height = _resolution.y * 0.333f;
        _refractionTexture->Bind();
        vec4<I32> viewport(
            _resolution.x * 0.333f,
            Application::getInstance().getResolution().y - height,
            _resolution.x * 0.666f, height);
        GFX_DEVICE.renderInViewport(
            viewport,
            DELEGATE_BIND(
                (void (GFXDevice::*)(U32, size_t, ShaderProgram* const)) &
                    GFXDevice::drawPoints,
                &GFX_DEVICE, 1, GFX_DEVICE.getDefaultStateBlock(true),
                _previewReflectionShader));
    }
#endif
    Reflector::previewReflection();
}
};