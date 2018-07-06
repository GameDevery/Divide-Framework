#include "Headers/SceneNode.h"

#include "Core/Math/Headers/Transform.h"
#include "Managers/Headers/SceneManager.h"
#include "Geometry/Shapes/Headers/Object3D.h"
#include "Geometry/Shapes/Headers/Mesh.h"
#include "Geometry/Shapes/Headers/SubMesh.h"
#include "Geometry/Material/Headers/Material.h"
#include "Hardware/Video/Shaders/Headers/ShaderManager.h"

namespace Divide {

SceneNode::SceneNode(const SceneNodeType& type) : SceneNode("default", type)
{
}

SceneNode::SceneNode(const stringImpl& name, const SceneNodeType& type) : Resource(name),
                                                             _material(nullptr),
                                                             _refreshMaterialData(true),
                                                             _nodeReady(false),
                                                             _type(type),
                                                             _lodLevel(0),
                                                             _LODcount(1), ///<Defaults to 1 LOD level
                                                             _sgnReferenceCount(0)
{
}

SceneNode::~SceneNode() {
}

void SceneNode::sceneUpdate(const U64 deltaTime, SceneGraphNode* const sgn, SceneState& sceneState){
    assert(_nodeReady);

    if (!_material) {
        return;
    }
    _refreshMaterialData = _material->isDirty();
    _material->clean();
}

bool SceneNode::getDrawState(const RenderStage& currentStage)  { 
    Material* mat = getMaterial();
    if (mat && !mat->getShaderInfo(currentStage).getProgram()->isHWInitComplete()) {
       return false;
    }

    return _renderState.getDrawState(currentStage); 
}

bool SceneNode::isInView(const SceneRenderState& sceneRenderState, const BoundingBox& boundingBox, const BoundingSphere& sphere, const bool distanceCheck){
    assert(_nodeReady);
    
    const Camera& cam = sceneRenderState.getCameraConst();
    const vec3<F32>& eye = cam.getEye();
    const vec3<F32>& center  = sphere.getCenter();
    F32  cameraDistance = center.distance(eye);
    F32 visibilityDistance = GET_ACTIVE_SCENE()->state().getGeneralVisibility() + sphere.getRadius();
    if(distanceCheck && cameraDistance > visibilityDistance) {
        if (boundingBox.nearestDistanceFromPointSquared(eye) > std::min(visibilityDistance, sceneRenderState.getCameraConst().getZPlanes().y)) {
            return false;
        }
    }

    if (!boundingBox.ContainsPoint(eye)) {
        switch (cam.getFrustumConst().ContainsSphere(center, sphere.getRadius())) {
            case Frustum::FRUSTUM_OUT: {
                return false;
            };
            case Frustum::FRUSTUM_INTERSECT:	{
                if (!cam.getFrustumConst().ContainsBoundingBox(boundingBox)) {
                    return false;
                }
            };
        }
    }

    _lodLevel = (cameraDistance > Config::SCENE_NODE_LOD0) ? ((cameraDistance > Config::SCENE_NODE_LOD1) ? 2 : 1) : 0;
    
    return true;
}

Material* const SceneNode::getMaterial() {
    //UpgradableReadLock ur_lock(_materialLock);
    if (_material == nullptr && !_renderState._noDefaultMaterial) {
        //UpgradeToWriteLock uw_lock(ur_lock);
        _material = CreateResource<Material>(ResourceDescriptor("defaultMaterial"));
        REGISTER_TRACKED_DEPENDENCY(_material);
    }
    return _material;
}

void SceneNode::setMaterial(Material* const mat) {
    if (mat) { //If we need to update the material
        //UpgradableReadLock ur_lock(_materialLock);
        if (_material) { //If we had an old material
            if (_material->getGUID() != mat->getGUID()) { //if the old material isn't the same as the new one
                PRINT_FN(Locale::get("REPLACE_MATERIAL"),_material->getName().c_str(),mat->getName().c_str());
                //UpgradeToWriteLock uw_lock(ur_lock);
                RemoveResource(_material);			//remove the old material
                UNREGISTER_TRACKED_DEPENDENCY(_material);
                //ur_lock.lock();
            }
        }
        //UpgradeToWriteLock uw_lock(ur_lock);
        _material = mat;				   //set the new material
        REGISTER_TRACKED_DEPENDENCY(_material);
    } else { //if we receive a null material, the we need to remove this node's material
        //UpgradableReadLock ur_lock(_materialLock);
        if (_material) {
            //UpgradeToWriteLock uw_lock(ur_lock);
            UNREGISTER_TRACKED_DEPENDENCY(_material);
            RemoveResource(_material);
        }
    }
}

ShaderProgram* const SceneNode::getDrawShader(RenderStage renderStage) {
    return (_material ? _material->getShaderInfo(renderStage).getProgram() : nullptr);
}

size_t SceneNode::getDrawStateHash(RenderStage renderStage){
    if (!_material) {
        return 0L;
    }

    bool depthPass = GFX_DEVICE.isCurrentRenderStage(DEPTH_STAGE);
    bool shadowStage = GFX_DEVICE.isCurrentRenderStage(SHADOW_STAGE);

    if (!_material && depthPass) {
        return shadowStage ? _renderState.getShadowStateBlock() : _renderState.getDepthStateBlock();
    }

    bool reflectionStage = GFX_DEVICE.isCurrentRenderStage(REFLECTION_STAGE);

    return _material->getRenderStateBlock(depthPass ? (shadowStage ? SHADOW_STAGE : Z_PRE_PASS_STAGE) :
                                                      (reflectionStage ? REFLECTION_STAGE : FINAL_STAGE));
}

void SceneNode::bindTextures() {
    if (getMaterial()) {
        getMaterial()->bindTextures();
    }
}

bool SceneNode::computeBoundingBox(SceneGraphNode* const sgn) {
    sgn->setInitialBoundingBox(sgn->getBoundingBoxConst());
    sgn->getBoundingBox().setComputed(true);
    return true;
}

bool SceneNode::unload(){
    setMaterial(nullptr);
    return true;
}

void SceneNode::drawBoundingBox(SceneGraphNode* const sgn) const {
    const BoundingBox& bb = sgn->getBoundingBoxConst();
    GFX_DEVICE.drawBox3D(bb.getMin(), bb.getMax(), vec4<U8>(0, 0, 255, 255));
}

};