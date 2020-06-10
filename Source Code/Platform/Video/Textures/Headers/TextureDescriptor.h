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
#ifndef _TEXTURE_DESCRIPTOR_H_
#define _TEXTURE_DESCRIPTOR_H_

#include "Core/Resources/Headers/ResourceDescriptor.h"
#include "Platform/Video/Headers/RenderAPIEnums.h"
#include "Utility/Headers/Colours.h"

namespace Divide {

/// This struct is used to define all of the sampler settings needed to use a texture
/// We do not define copy constructors as we must define descriptors only with POD
struct SamplerDescriptor final : Hashable {
   
    SamplerDescriptor();
    ~SamplerDescriptor() = default;

    size_t getHash() const noexcept override;

    void wrapUVW(const TextureWrap wrap) noexcept {
        wrapU(wrap); wrapV(wrap); wrapW(wrap);
    }

    /// Texture filtering mode
    PROPERTY_RW(TextureFilter, minFilter, TextureFilter::LINEAR_MIPMAP_LINEAR);
    PROPERTY_RW(TextureFilter, magFilter, TextureFilter::LINEAR);
    /// Texture wrap mode (Or S-R-T)
    PROPERTY_RW(TextureWrap, wrapU, TextureWrap::REPEAT);
    PROPERTY_RW(TextureWrap, wrapV, TextureWrap::REPEAT);
    PROPERTY_RW(TextureWrap, wrapW, TextureWrap::REPEAT);
    ///use red channel as comparison (e.g. for shadows)
    PROPERTY_RW(bool, useRefCompare, false);
    ///Used by RefCompare
    PROPERTY_RW(ComparisonFunction, cmpFunc, ComparisonFunction::LEQUAL);
    /// The value must be in the range [0...255] and is automatically clamped by the max HW supported level
    PROPERTY_RW(U8, anisotropyLevel, 255);
    /// OpenGL eg: used by TEXTURE_MIN_LOD and TEXTURE_MAX_LOD
    PROPERTY_RW(I32, minLOD, -1000);
    PROPERTY_RW(I32, maxLOD, 1000);
    /// OpenGL eg: used by TEXTURE_LOD_BIAS
    PROPERTY_RW(I32, biasLOD, 0);
    /// Used with CLAMP_TO_BORDER as the background colour outside of the texture border
    PROPERTY_RW(FColour4, borderColour, DefaultColours::BLACK);

public:
    static void clear();
    /// Retrieve a sampler descriptor by hash value.
    /// If the hash value doesn't exist in the descriptor map, return the default descriptor
    static const SamplerDescriptor& get(size_t samplerDescriptorHash);
    /// Returns false if the specified hash is not found in the map
    static const SamplerDescriptor& get(size_t samplerDescriptorHash, bool& descriptorFound);

protected:
    using SamplerDescriptorMap = hashMap<size_t, SamplerDescriptor, NoHash<size_t>>;
    static SamplerDescriptorMap s_samplerDescriptorMap;
    static SharedMutex s_samplerDescriptorMapMutex;

public:
    static size_t s_defaultHashValue;
};

/// Use to define a texture with details such as type, image formats, etc
/// We do not define copy constructors as we must define descriptors only with POD
class TextureDescriptor final : public PropertyDescriptor {
    friend class Texture;

   public:
    TextureDescriptor() noexcept
        : TextureDescriptor(TextureType::TEXTURE_2D)
    {
    }

    TextureDescriptor(const TextureType type) noexcept
         : TextureDescriptor(type,
                             GFXImageFormat::RGB,
                             GFXDataFormat::UNSIGNED_BYTE)
    {
    }

    TextureDescriptor(const TextureType type,
                      const GFXImageFormat format,
                      const GFXDataFormat dataType) noexcept
        : PropertyDescriptor(DescriptorType::DESCRIPTOR_TEXTURE),
          _mipLevels(0u, 1u),
          _dataType(dataType),
          _baseFormat(format),
          _texType(type)
    {
    }

    bool isCubeTexture() const noexcept {
        return (_texType == TextureType::TEXTURE_CUBE_MAP ||
                _texType == TextureType::TEXTURE_CUBE_ARRAY);
    }

    bool isArrayTexture() const noexcept {
        return _texType == TextureType::TEXTURE_2D_ARRAY;
    }

    bool isMultisampledTexture() const noexcept {
        return _texType == TextureType::TEXTURE_2D_MS ||
               _texType == TextureType::TEXTURE_2D_ARRAY_MS;
    }

    U8 numChannels() const noexcept {
        switch (baseFormat()) {
                case GFXImageFormat::RED:
                case GFXImageFormat::DEPTH_COMPONENT:
                    return 1;
                case GFXImageFormat::RG:
                    return 2;
                case GFXImageFormat::BGR:
                case GFXImageFormat::RGB:
                    return 3;
                case GFXImageFormat::BGRA:
                case GFXImageFormat::RGBA:
                    return 4;
                default: break;
        }

        return 0;
    }

    const vectorEASTL<stringImpl>& sourceFileList() const noexcept { return _sourceFileList; }

    size_t getHash() const noexcept final;

    PROPERTY_RW(U16, layerCount, 1);
    PROPERTY_RW(vec2<U16>, mipLevels);
    PROPERTY_RW(U16, mipCount, 1);
    PROPERTY_RW(U8, msaaSamples, 0);
    PROPERTY_RW(GFXDataFormat, dataType, GFXDataFormat::COUNT);
    PROPERTY_RW(GFXImageFormat, baseFormat, GFXImageFormat::COUNT);
    PROPERTY_RW(TextureType, texType, TextureType::COUNT);
    /// Automatically compute mip maps (overwrites any manual mipmap computation)
    PROPERTY_RW(bool, autoMipMaps, true);
    PROPERTY_RW(bool, hasMipMaps, true);
    /// Use SRGB colour space
    PROPERTY_RW(bool, srgb, false);
    PROPERTY_RW(bool, normalized, true);
    PROPERTY_RW(bool, compressed, false);

   private:
       vectorEASTL<stringImpl> _sourceFileList;
};

namespace XMLParser {
    void saveToXML(const SamplerDescriptor& sampler, const stringImpl& entryName, boost::property_tree::ptree& pt);
    size_t loadFromXML(const stringImpl& entryName, const boost::property_tree::ptree& pt);
};
};  // namespace Divide
#endif
