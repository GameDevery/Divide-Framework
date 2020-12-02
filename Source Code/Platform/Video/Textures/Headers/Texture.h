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
#ifndef _TEXTURE_H_
#define _TEXTURE_H_

#include "Core/Resources/Headers/Resource.h"

#include "Platform/Video/Headers/GraphicsResource.h"
#include "Platform/Video/Headers/RenderAPIWrapper.h"
#include "Platform/Video/Textures/Headers/TextureDescriptor.h"

#include "Utility/Headers/ImageTools.h"

namespace Divide {

namespace TypeUtil {
    const char* WrapModeToString(TextureWrap wrapMode) noexcept;
    TextureWrap StringToWrapMode(const stringImpl& wrapMode);

    const char* TextureFilterToString(TextureFilter filter) noexcept;
    TextureFilter StringToTextureFilter(const stringImpl& filter);
};

 TYPEDEF_SMART_POINTERS_FOR_TYPE(Texture);

/// An API-independent representation of a texture
class NOINITVTABLE Texture : public CachedResource, public GraphicsResource {
    friend class ResourceCache;
    friend class ResourceLoader;
    template <typename T>
    friend class ImplResourceLoader;
  public:

    explicit Texture(GFXDevice& context,
                     size_t descriptorHash,
                     const Str256& name,
                     const ResourcePath& assetNames,
                     const ResourcePath& assetLocations,
                     bool isFlipped,
                     bool asyncLoad,
                     const TextureDescriptor& texDescriptor);

    virtual ~Texture() = default;

    // Returns the GPU address of the texture
    virtual U64 makeResident(size_t samplerHash) = 0;

    /// Bind a single level
    virtual void bindLayer(U8 slot, U8 level, U8 layer, bool layered, Image::Flag rw_flag) = 0;

    /// Resize the texture to the specified dimensions and upload the new data
    virtual void resize(const std::pair<Byte*, size_t>& ptr, const vec2<U16>& dimensions) = 0;
    /// Change the number of MSAA samples for this current texture
    void setSampleCount(U8 newSampleCount) noexcept;

    // API-dependent loading function that uploads ptr data to the GPU using the
    // specified parameters
    virtual void loadData(const ImageTools::ImageData& imageData) = 0;
    virtual void loadData(const std::pair<Byte*, size_t>& data, const vec2<U16>& dimensions) = 0;

    virtual void clearData(const UColour4& clearColour, U8 level) const = 0;
    virtual void clearSubData(const UColour4& clearColour, U8 level, const vec4<I32>& rectToClear, const vec2<I32>& depthRange) const = 0;

    // MipLevel will automatically clamped to the texture's internal limits
    virtual std::pair<std::shared_ptr<Byte[]>, size_t> readData(U16 mipLevel, GFXDataFormat desiredFormat = GFXDataFormat::COUNT) const = 0;

    PROPERTY_R(TextureDescriptor, descriptor);
    PROPERTY_R(TextureData, data);
    /// Set/Get the number of layers (used by texture arrays)
    PROPERTY_RW(U32, numLayers, 1u);
    /// Texture width as returned by STB/DDS loader
    PROPERTY_R(U16, width, 0u);
    /// Texture height depth as returned by STB/DDS loader
    PROPERTY_R(U16, height, 0u);
    /// If the texture has an alpha channel and at least one pixel is translucent, return true
    PROPERTY_R(bool, hasTranslucency, false);
    /// If the texture has an alpha channel and at least on pixel is fully transparent and no pixels are partially transparent, return true
    PROPERTY_R(bool, hasTransparency, false);
    /// Flipped Y-coord
    PROPERTY_R(bool, flipped, false);

    static U16 ComputeMipCount(U16 width, U16 height) noexcept;
#if defined(_DEBUG)
    const vectorEASTL<stringImpl>& sourceFileList() const noexcept { return _sourceFileList; }
#endif
   protected:
    /// Use STB/NV_DDS to load a file into a Texture Object
    bool loadFile(const ResourcePath& name, ImageTools::ImageData& fileData);
    bool checkTransparency(const ResourcePath& name, ImageTools::ImageData& fileData);
    /// Load texture data using the specified file name
    bool load() override;
    virtual void threadedLoad();

    virtual void validateDescriptor();

    [[nodiscard]] const char* getResourceTypeName() const noexcept override { return "Texture"; }

  protected:
    bool _asyncLoad;
    vectorEASTL<stringImpl> _sourceFileList;
#if defined(_DEBUG)
  protected:
    static const char* s_missingTextureFileName;
#endif
};

};  // namespace Divide
#endif // _TEXTURE_H_
