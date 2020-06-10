/*
   Copyright (c) 2018 DIVIDE-Studio
   Copyright (c) 2009 Ionut Cava

   This file is part of DIVIDE Framework.

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software
   and associated documentation files (the "Software"), to deal in the Software
   without restriction,
   including without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED,
   INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
   PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
   DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
   IN CONNECTION WITH THE SOFTWARE
   OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

#pragma once
#ifndef _RENDERER_H_
#define _RENDERER_H_

#include "Platform/Video/Headers/GFXDevice.h"

namespace Divide {

class PostFX;
class LightPool;
class ResourceCache;
class PlatformContext;

/// TiledForwardShading
class Renderer : public PlatformContextComponent {
   public:
    Renderer(PlatformContext& context, ResourceCache* cache);
    ~Renderer();

    void preRender(RenderStagePass stagePass,
                   const Texture_ptr& hizColourTexture,
                   const size_t samplerHash,
                   LightPool& lightPool,
                   const Camera* camera,
                   GFX::CommandBuffer& bufferInOut) const;

    void idle() const;

    void updateResolution(U16 newWidth, U16 newHeight) const;

    PostFX& postFX() noexcept { return *_postFX; }

    const PostFX& postFX() const noexcept { return *_postFX; }

  private:
    ResourceCache* _resCache = nullptr;

    ShaderProgram_ptr _lightCullComputeShader = nullptr;
    ShaderBuffer*     _perTileLightIndexBuffer = nullptr;
    Pipeline*         _lightCullPipeline = nullptr;
    eastl::unique_ptr<PostFX> _postFX = nullptr;
};

};  // namespace Divide

#endif