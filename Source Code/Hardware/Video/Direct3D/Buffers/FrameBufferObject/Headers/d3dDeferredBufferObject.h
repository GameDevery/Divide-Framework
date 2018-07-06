/*
   Copyright (c) 2013 DIVIDE-Studio
   Copyright (c) 2009 Ionut Cava

   This file is part of DIVIDE Framework.
   
   Permission is hereby granted, free of charge, to any person obtaining a copy of this software
   and associated documentation files (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
   and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, 
   subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
   INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE 
   OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

#ifndef _D3D_DEFERRED_BUFFER_OBJECT_H
#define _D3D_DEFERRED_BUFFER_OBJECT_H

#include "d3dFrameBufferObject.h"

class d3dDeferredBufferObject : public d3dFrameBufferObject {
public:

	d3dDeferredBufferObject();
	~d3dDeferredBufferObject() {Destroy();}

	bool Create(U16 width, U16 height, U8 imageLayers = 0) {return true;}

	void Destroy() {}

	void Begin(U8 nFace=0) const {}
	void End(U8 nFace=0) const {}

	void Bind(U8 unit=0, U8 texture = 0) {}
	void Unbind(U8 unit=0) {}

private:
	vectorImpl<U32> _textureId;
};

#endif