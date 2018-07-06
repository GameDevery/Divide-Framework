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

#ifndef _BOX_3D_H_
#define _BOX_3D_H_
#include "Geometry/Object3D.h"

class Box3D : public Object3D
{
public:
	Box3D(F32 size) :  Object3D(BOX_3D), _size(size) {}
	bool load(const std::string &name) {_name = name; return true;}

	F32&         getSize()    {return _size;}

	virtual bool computeBoundingBox(){
		_boundingBox.set(vec3(-_size,-_size,-_size),vec3(_size,_size,_size));
		_boundingBox.Multiply(0.5f);
		_boundingBox.isComputed() = true;
		setOriginalBoundingBox(_boundingBox);
		return true;
	}
private:
	F32 _size;
};


#endif