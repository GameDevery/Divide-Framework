/*
   Copyright (c) 2015 DIVIDE-Studio
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

#ifndef _WRAPPER_DX_H_
#define _WRAPPER_DX_H_

#include "Platform/Video/Headers/RenderAPIWrapper.h"

#include "Platform/Video/Direct3D/Buffers/Framebuffer/Headers/d3dRenderTarget.h"
#include "Platform/Video/Direct3D/Buffers/VertexBuffer/Headers/d3dVertexBuffer.h"
#include "Platform/Video/Direct3D/Buffers/VertexBuffer/Headers/d3dGenericVertexData.h"
#include "Platform/Video/Direct3D/Buffers/ShaderBuffer/Headers/d3dConstantBuffer.h"
#include "Platform/Video/Direct3D/Buffers/PixelBuffer/Headers/d3dPixelBuffer.h"
#include "Platform/Video/Direct3D/Shaders/Headers/d3dShaderProgram.h"
#include "Platform/Video/Direct3D/Shaders/Headers/d3dShader.h"
#include "Platform/Video/Direct3D/Textures/Headers/d3dTexture.h"
#include "Platform/Video/Direct3D/Headers/d3dEnumTable.h"

namespace Divide {

DEFINE_SINGLETON_EXT1_W_SPECIFIER(DX_API, RenderAPIWrapper, final)
  protected:
    DX_API() : RenderAPIWrapper() {}
    virtual ~DX_API() {}

    ErrorCode initRenderingAPI(I32 argc, char** argv) override;
    void closeRenderingAPI() override;
    void changeResolution(U16 w, U16 h) override;
    void changeViewport(const vec4<I32>& newViewport) const override;
    void setCursorPosition(I32 x, I32 y) override;
    void registerCommandBuffer(const ShaderBuffer& commandBuffer) const override;
    bool makeTexturesResident(const TextureDataContainer& textureData) override;
    bool makeTextureResident(const TextureData& textureData) override;
    void beginFrame() override;
    void endFrame() override;

    inline ShaderBuffer* newSB(GFXDevice& context,
                               const stringImpl& bufferName,
                               const U32 ringBufferLength = 1,
                               const bool unbound = false,
                               const bool persistentMapped = true,
                               BufferUpdateFrequency frequency =
                                   BufferUpdateFrequency::ONCE) const override {
        return MemoryManager_NEW d3dConstantBuffer(context,
                                                   bufferName, 
                                                   ringBufferLength,
                                                   unbound,
                                                   persistentMapped,
                                                   frequency);
    }

    inline IMPrimitive* newIMP(GFXDevice& context) const override { return nullptr; }

    inline Framebuffer* newFB(GFXDevice& context, bool multisampled) const override {
        return MemoryManager_NEW d3dRenderTarget(context, multisampled);
    }

    inline GenericVertexData* newGVD(GFXDevice& context,
                                     const bool persistentMapped = false,
                                     const U32 ringBufferLength = 1) const override {
        return MemoryManager_NEW d3dGenericVertexData(context, persistentMapped, ringBufferLength);
    }

    inline VertexBuffer* newVB(GFXDevice& context) const override {
        return MemoryManager_NEW d3dVertexBuffer(context);
    }

    inline PixelBuffer* newPB(GFXDevice& context, const PBType& type) const override {
        return MemoryManager_NEW d3dPixelBuffer(context, type);
    }

    inline Texture* newTexture(GFXDevice& context, TextureType type, bool asyncLoad) const override {
        return MemoryManager_NEW d3dTexture(context, type, asyncLoad);
    }

    inline ShaderProgram* newShaderProgram(GFXDevice& context, bool asyncLoad) const override {
        return MemoryManager_NEW d3dShaderProgram(context, asyncLoad);
    }

    inline Shader* newShader(GFXDevice& context,
                             const stringImpl& name,
                             const ShaderType& type,
                             const bool optimise = false) const override {
        return MemoryManager_NEW d3dShader(context, name, type, optimise);
    }

    bool initShaders() override;
    bool deInitShaders() override;

    void toggleDepthWrites(bool state) override;
    void toggleRasterization(bool state) override;

    void drawText(const TextLabel& textLabel, const vec2<F32>& relativeOffset) override;
    void drawPoints(U32 numPoints) override;
    void drawTriangle() override;
    void updateClipPlanes() override;
    void threadedLoadCallback() override;

    U64 getFrameDurationGPU() override { return 0; }
    void activateStateBlock(const RenderStateBlock& newBlock,
                            const RenderStateBlock& oldBlock) const override;

END_SINGLETON

};  // namespace Divide
#endif
