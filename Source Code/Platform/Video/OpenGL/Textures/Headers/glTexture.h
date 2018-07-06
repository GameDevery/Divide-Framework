/*
   Copyright (c) 2016 DIVIDE-Studio
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

#ifndef _GL_TEXTURE_H_
#define _GL_TEXTURE_H_

#include "Platform/Video/Textures/Headers/Texture.h"

namespace Divide {
class glLockManager;
class glTexture final : public Texture {
   public:
    explicit glTexture(GFXDevice& context,
                       const stringImpl& name,
                       const stringImpl& resourceLocation,
                       TextureType type,
                       bool asyncLoad);
    ~glTexture();

    bool unload() override;

    void Bind(U8 unit, bool flushStateOnRequest = true) override;

    void BindLayer(U8 slot, U8 level, U8 layer, bool layered, bool read, bool write, bool flushStateOnRequest = true) override;

    void setMipMapRange(GLushort base = 0, GLushort max = 1000) override;

    void resize(const bufferPtr ptr,
                const vec2<U16>& dimensions,
                const vec2<U16>& mipLevels) override;

    void loadData(const TextureLoadInfo& info,
                  const TextureDescriptor& descriptor,
                  const vectorImpl<ImageTools::ImageLayer>& imageLayers,
                  const vec2<GLushort>& mipLevels) override;
    void loadData(const TextureLoadInfo& info,
                  const TextureDescriptor& descriptor,
                  const bufferPtr data,
                  const vec2<U16>& dimensions,
                  const vec2<U16>& mipLevels) override;

    bool flushTextureState() override;

   protected:
    void threadedLoad() override;
    void reserveStorage(const TextureLoadInfo& info);
    void updateMipMaps();
    void updateSampler();

    void loadDataCompressed(const TextureLoadInfo& info,
                            const vectorImpl<ImageTools::ImageLayer>& imageLayers);

    void loadDataUncompressed(const TextureLoadInfo& info,
                              bufferPtr data);
   private:
    GLenum _type;
    std::atomic_bool _allocatedStorage;
    glLockManager* _lockManager;
};

};  // namespace Divide

#endif