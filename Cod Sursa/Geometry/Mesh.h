#ifndef _MESH_H_
#define _MESH_H_

/*
DIVIDE-Engine: 21.10.2010 (Ionut Cava)

Mesh class. This class wraps all of the renderable geometry drawn by the engine. 
The only exceptions are: Terrain (including TerrainChunk) and Vegetation.

Meshes are composed of at least 1 submesh that contains vertex data, texture info and so on.
A mesh has a name, position, rotation, scale and a boolean value that enables or disables rendering 
across the network and one that disables rendering alltogheter;

Note: all transformations applied to the mesh affect every submesh that compose the mesh. 
*/

#include "resource.h"
#include "SubMesh.h"

class Mesh : public Object3D
{

public:
	Mesh() : Object3D() {_geometryType = MESH;}
	Mesh(const vec3& position,const vec3& scale,const vec3& orientation,const Material& mat)
		: Object3D(position,scale,orientation,mat) {_geometryType = MESH;}
	Mesh(const Mesh& old);

	void addSubMesh(SubMesh* subMesh){_subMeshes.push_back(subMesh);}
	
	bool optimizeSubMeshes();
	
	inline std::vector<SubMesh*>&   getSubMeshes()   {return _subMeshes;}
	inline SubMesh*				    getSubMesh(const std::string& name);

	bool				getVisibility();
	void				onDraw();

	bool load(const std::string& file);
	bool unload();
	bool clean();

protected:
	bool isInView();
	void computeBoundingBox();
	void computeLightShaders();

protected:
	
	bool							  _visibleToNetwork, _loaded;
	std::vector<SubMesh* >		      _subMeshes;
	std::vector<SubMesh* >::iterator  _subMeshIterator;
};

#endif