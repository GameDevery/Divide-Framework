#include "Mesh.h"
#include "Managers/ResourceManager.h"
#include "Managers/SceneManager.h"
#include "Utility/Headers/Quaternion.h"
using namespace std;

bool Mesh::load(const string& name){
	_name = name;
	return true;
}

void Mesh::removeCopy(){
	SceneNode::removeCopy();
	//Removing a SceneGraphNode removes it's children. For a mesh, the children are the SubMeshes
	//for_each(std::string& it, _subMeshes){
	//	Resource* s = ResourceManager::getInstance().find(it);
	//	if(s) s->removeCopy();
	//}
}

void Mesh::createCopy(){
	SceneNode::createCopy();
	for_each(std::string& it, _subMeshes){
		Resource* s = ResourceManager::getInstance().find(it);
		if(s) s->createCopy();
	}
}

bool Mesh::computeBoundingBox(SceneGraphNode* node){
	BoundingBox& bb = node->getBoundingBox();
	if(bb.isComputed()) return true;
	bb.set(vec3(100000.0f, 100000.0f, 100000.0f),vec3(-100000.0f, -100000.0f, -100000.0f));
	for_each(childrenNodes::value_type& it, node->getChildren()){
		it.second->getBoundingBox().Transform(it.second->getInitialBoundingBox(), it.second->getTransform()->getMatrix());
		bb.Add(it.second->getBoundingBox());
	}
	return SceneNode::computeBoundingBox(node);
}

void Mesh::postLoad(SceneGraphNode* node){
	for_each(std::string& it, _subMeshes){
		ResourceDescriptor subMesh(it);
		SubMesh* s = dynamic_cast<SubMesh*>(ResourceManager::getInstance().find(it));
		if(!s) continue;
		SceneGraphNode* sGN  = node->addNode(s,node->getName()+"_"+it);
		//Set submesh transform to this transform
		sGN->getTransform()->setTransforms(node->getTransform()->getMatrix());
	}
	Object3D::postLoad(node);
}

bool Mesh::clean()
{
	if(_shouldDelete)
		return SceneManager::getInstance().getActiveScene()->removeGeometry(this);
	else return false;
}