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

#ifndef _SSAO_PRE_RENDER_OPERATOR_H_
#define _SSAO_PRE_RENDER_OPERATOR_H_

#include "Rendering/PostFX/Headers/PreRenderOperator.h"

namespace Divide {

class SSAOPreRenderOperator : public PreRenderOperator {
   public:
    SSAOPreRenderOperator(GFXDevice& context, PreRenderBatch& parent, ResourceCache& cache);
    ~SSAOPreRenderOperator();

    void idle(const Configuration& config) override;
    void execute(const Camera& camera, GFX::CommandBuffer& bufferInOut) override;
    void reshape(U16 width, U16 height) override;
    TextureData getDebugOutput() const;

   private:
    RenderTargetHandle _ssaoOutput;
    RenderTargetHandle _ssaoOutputBlurred;
    ShaderProgram_ptr _ssaoGenerateShader;
    ShaderProgram_ptr _ssaoApplyShader;
    ShaderProgram_ptr _ssaoBlurShader;
    Texture_ptr _noiseTexture;
    PushConstants _ssaoBlurConstants;
    PushConstants _ssaoGenerateConstants;
};

};  // namespace Divide

#endif