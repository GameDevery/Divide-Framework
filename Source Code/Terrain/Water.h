/*�Copyright 2009-2011 DIVIDE-Studio�*/
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

#ifndef _WATER_PLANE_H_
#define _WATER_PLANE_H_

#include "resource.h"
#include "Geometry/Object3D.h"
#include "Geometry/Predefined/Quad3D.h"

#include "Hardware/Video/FrameBufferObject.h"
#include "Utility/Headers/ParamHandler.h"

class Shader;
class WaterPlane : public SceneNode
{
public:
	WaterPlane();
	~WaterPlane(){}
	void render(SceneGraphNode* node);
	bool load(const std::string& name);
	bool unload();

	void setParams(F32 shininess, F32 noiseTile, F32 noiseFactor, F32 transparency);
	inline Quad3D*     getQuad()    {return _plane;}
	inline FrameBufferObject* getReflectionFBO(){return _reflectionFBO;}
	bool   isInView(bool distanceCheck,BoundingBox& boundingBox) {return true;}
	void   postLoad(SceneGraphNode* node);
	void   prepareMaterial();
	void   releaseMaterial();
private:
	bool computeBoundingBox(SceneGraphNode* node);

private:
	Quad3D*			   _plane;
	Texture2D*		   _texture;
	Shader*		  	   _shader;
	F32				   _farPlane;
	FrameBufferObject* _reflectionFBO;
	Transform*         _planeTransform;
	SceneGraphNode*    _node;
	SceneGraphNode*    _planeSGN;
};
#endif