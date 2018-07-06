#include "Core/Resources/Headers/ResourceLoader.h"
#include "Core/Resources/Headers/ResourceCache.h"
#include "Geometry/Shapes/Headers/Mesh.h"
#include "Geometry/Importer/Headers/DVDConverter.h"

Mesh* ImplResourceLoader<Mesh>::operator()(){

	Mesh* ptr = DVDConverter::getInstance().load(_descriptor.getResourceLocation());
	
	if(!ptr) return NULL;
	if(!load(ptr,_descriptor.getName())) return NULL;
	if(_descriptor.getFlag()){
		ptr->getSceneNodeRenderState().useDefaultMaterial(false);
		ptr->setMaterial(NULL);
	}
	ptr->setResourceLocation(_descriptor.getResourceLocation());

	return ptr;
}

DEFAULT_LOADER_IMPL(Mesh)