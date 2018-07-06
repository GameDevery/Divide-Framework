/*
   Copyright (c) 2017 DIVIDE-Studio
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

/// shader from:
/// http://www.geeks3d.com/20110405/fxaa-fast-approximate-anti-aliasing-demo-glsl-opengl-test-radeon-geforce/

#ifndef _POST_AA_PRE_RENDER_OPERATOR_H_
#define _POST_AA_PRE_RENDER_OPERATOR_H_

#include "Rendering/PostFX/Headers/PreRenderOperator.h"

namespace Divide {

class PostAAPreRenderOperator : public PreRenderOperator {
   public:
    PostAAPreRenderOperator(GFXDevice& context, PreRenderBatch& parent, ResourceCache& cache);
    ~PostAAPreRenderOperator();

    void idle(const Configuration& config) override;
    void execute(GFX::CommandBuffer& bufferInOut) override;
    void reshape(U16 width, U16 height) override;

   private:
    I32 _idleCount;
    bool _useSMAA;
    I32 _postAASamples;
    ShaderProgram_ptr _fxaa;
    ShaderProgram_ptr _smaa;
    PushConstants _fxaaConstants;
};

};  // namespace Divide

#endif