/*�Copyright 2009-2012 DIVIDE-Studio�*/
/* This file is part of DIVIDE Framework.

   DIVIDE Framework is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   DIVIDE Framework is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with DIVIDE Framework.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _OBJECT_3D_H_
#define _OBJECT_3D_H_

#include "core.h"
#include "Hardware/Video/GFXDevice.h"
#include "Graphs/Headers/SceneGraphNode.h"

enum PrimitiveType {

	OBJECT_3D,
	SPHERE_3D,
	BOX_3D,
	QUAD_3D,
	TEXT_3D,
	MESH,
	SUBMESH,
	GENERIC,
	OBJECT_3D_FLYWEIGHT
};

class BoundingBox;

class Object3D : public SceneNode {
public:
	Object3D(PrimitiveType type = OBJECT_3D) : SceneNode(TYPE_OBJECT3D),
											  _update(false),
											  _geometryType(type),
											  _geometry(GFX_DEVICE.newVBO()),
											  _refreshVBO(true)

	{}

	Object3D(const std::string& name, PrimitiveType type = OBJECT_3D) : SceneNode(name,TYPE_OBJECT3D),
																	    _update(false),
																		_geometryType(type),
																		_geometry(GFX_DEVICE.newVBO()),
																		_refreshVBO(true)
	{}

	virtual ~Object3D(){
		SAFE_DELETE(_geometry);
	};


	virtual void						postLoad(SceneGraphNode* const sgn) {}	
	inline  PrimitiveType               getType()       const {return _geometryType;}
	
	virtual		void    render(SceneGraphNode* const sgn);

	inline VertexBufferObject* const getGeometryVBO() {return _geometry;} ///<Please use IBO's ...
	inline vec2<U16>&          getIndiceLimits()      {return _indiceLimits; }
	virtual void onDraw();

	/// Called from SceneGraph "sceneUpdate"
	virtual void sceneUpdate(D32 sceneTime) {}

protected:
	virtual void computeNormals() {};
	virtual void computeTangents();

protected:
	bool		          _update, _refreshVBO;
	PrimitiveType         _geometryType;
	VertexBufferObject*   _geometry;
	vec2<U16>             _indiceLimits;
};

#endif