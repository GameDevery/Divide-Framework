#include "Headers/Mesh.h"
#include "Headers/SubMesh.h"

#include "Managers/Headers/SceneManager.h"
#include "Core/Headers/ParamHandler.h"
#include "Core/Math/Headers/Transform.h"


void Mesh::updateBBatCurrentFrame(SceneGraphNode* const sgn){
    if(!ParamHandler::getInstance().getParam<bool>("mesh.playAnimations"))
        return;

    if(sgn->updateBB()){
        BoundingBox& bb = sgn->getBoundingBox();
        bb.setComputed(false);
    }

    SceneNode::updateBBatCurrentFrame(sgn);
}

/// Mesh bounding box is built from all the SubMesh bounding boxes
bool Mesh::computeBoundingBox(SceneGraphNode* const sgn){
    BoundingBox& bb = sgn->getBoundingBox();

    bb.reset();
    for_each(childrenNodes::value_type& s, sgn->getChildren()){
        bb.Add(s.second->getBoundingBox());
    }
    return SceneNode::computeBoundingBox(sgn);
}

#pragma message("Set SubMesh transform to Mesh transform - HACK. ToDo <- Fix this. Use parent matrix")
/// After we loaded our mesh, we need to add submeshes as children nodes
void Mesh::postLoad(SceneGraphNode* const sgn){
    for_each(std::string& it, _subMeshes){
        ResourceDescriptor subMesh(it);
        /// Find the SubMesh resource
        SubMesh* s = FindResource<SubMesh>(it);
        if(!s) continue;
        REGISTER_TRACKED_DEPENDENCY(s);

        /// Add the SubMesh resource as a child
        SceneGraphNode* subMeshSGN  = sgn->addNode(s,sgn->getName()+"_"+it);
        subMeshSGN->getTransform()->setParentTransform(sgn->getTransform());
        ///Hold a reference to the submesh by ID (used for animations)
        _subMeshRefMap.insert(std::make_pair(s->getId(), s));
        s->setParentMesh(this);
    }
    Object3D::postLoad(sgn);
}

void Mesh::onDraw(const RenderStage& currentStage){
    if(getState() != RES_LOADED) return;
    Object3D::onDraw(currentStage);
}

void Mesh::updateTransform(SceneGraphNode* const sgn){
    SceneNode::updateTransform(sgn);
}

/// Called from SceneGraph "sceneUpdate"
void Mesh::sceneUpdate(const U32 sceneTime,SceneGraphNode* const sgn, SceneState& sceneState){
    Object3D::sceneUpdate(sceneTime,sgn,sceneState);
}