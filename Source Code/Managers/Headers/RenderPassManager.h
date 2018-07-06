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

#ifndef _RENDER_PASS_MANAGER_H_
#define _RENDER_PASS_MANAGER_H_

#include "core.h"
class RenderPass;

struct RenderPassItem{
	RenderPass* _rp;
	U8 _sortKey;
	RenderPassItem(U8 sortKey, RenderPass *rp ) : _rp(rp), _sortKey(sortKey)
	{
	}
};

class SceneRenderState;

DEFINE_SINGLETON (RenderPassManager)

public:
	///Call every renderqueue's render function in order
	void render(SceneRenderState* const sceneRenderState = NULL);
	///Add a new pass with the specified key
	void addRenderPass(RenderPass* const renderPass, U8 orderKey);
	///Remove a renderpass from the manager, optionally not deleting it
	void removeRenderPass(RenderPass* const renderPass,bool deleteRP = true);
	///Find a renderpass by name and remove it from the manager, optionally not deleting it
	void removeRenderPass(const std::string& name,bool deleteRP = true);
	U16 getLastTotalBinSize(U8 renderPassId);

private:
	~RenderPassManager();
	vectorImpl<RenderPassItem > _renderPasses;

END_SINGLETON

#endif
