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
#ifndef _SSAO_PRE_RENDER_OPERATOR_H_
#define _SSAO_PRE_RENDER_OPERATOR_H_

#include "Rendering/PostFX/Headers/PreRenderOperator.h"

namespace Divide {

class SSAOPreRenderOperator final : public PreRenderOperator {
   public:
    SSAOPreRenderOperator(GFXDevice& context, PreRenderBatch& parent, ResourceCache* cache);
    ~SSAOPreRenderOperator();

    void prepare(const Camera* camera, GFX::CommandBuffer& bufferInOut) override;

    [[nodiscard]] bool execute(const Camera* camera, const RenderTargetHandle& input, const RenderTargetHandle& output, GFX::CommandBuffer& bufferInOut) override;
    void reshape(U16 width, U16 height) override;

    [[nodiscard]] F32 radius() const noexcept { return _radius[_genHalfRes ? 1 : 0]; }
    void radius(F32 val);

    [[nodiscard]] F32 power() const noexcept { return _power[_genHalfRes ? 1 : 0]; }
    void power(F32 val);

    [[nodiscard]] F32 bias() const noexcept { return _bias[_genHalfRes ? 1 : 0]; }
    void bias(F32 val);

    [[nodiscard]] bool genHalfRes() const noexcept { return _genHalfRes; }
    void genHalfRes(bool state);

    [[nodiscard]] bool blurResults() const noexcept { return _blur[_genHalfRes ? 1 : 0]; }
    void blurResults(bool state);

    [[nodiscard]] F32 blurThreshold() const noexcept { return _blurThreshold[_genHalfRes ? 1 : 0]; }
    void blurThreshold(F32 val);

    [[nodiscard]] U8 sampleCount() const noexcept;

    [[nodiscard]] bool ready() const override;

   private:
     void onToggle(bool state) override;

   private:
    PushConstants _ssaoGenerateConstants;
    PushConstants _ssaoBlurConstants;
    ShaderProgram_ptr _ssaoGenerateShader = nullptr;
    ShaderProgram_ptr _ssaoGenerateHalfResShader = nullptr;
    ShaderProgram_ptr _ssaoBlurShader = nullptr;
    ShaderProgram_ptr _ssaoPassThroughShader = nullptr;
    ShaderProgram_ptr _ssaoDownSampleShader = nullptr;
    ShaderProgram_ptr _ssaoUpSampleShader = nullptr;
    Texture_ptr _noiseTexture = nullptr;
    size_t _noiseSampler = 0;
    RenderTargetHandle _ssaoOutput;
    RenderTargetHandle _ssaoHalfResOutput;
    RenderTargetHandle _halfDepthAndNormals;
    F32 _radius[2] = { 0.0f, 0.0f };
    F32 _bias[2] = { 0.0f, 0.0f };
    F32 _power[2] = { 0.0f, 0.0f };
    F32 _blurThreshold[2] = { 0.05f, 0.05f };
    U8 _kernelSampleCount[2] = { 0u, 0u };
    bool _blur[2] = { false, false };

    bool _genHalfRes = false;
    bool _enabled = true;
};

}  // namespace Divide

#endif