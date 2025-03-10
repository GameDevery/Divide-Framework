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
#ifndef DVD_TEXTURE_DESCRIPTOR_INL_
#define DVD_TEXTURE_DESCRIPTOR_INL_

namespace Divide {

inline bool Is1DTexture( const TextureType texType ) noexcept
{
    return texType == TextureType::TEXTURE_1D ||
           texType == TextureType::TEXTURE_1D_ARRAY;
}

inline bool Is2DTexture( const TextureType texType ) noexcept
{
    return texType == TextureType::TEXTURE_2D ||
           texType == TextureType::TEXTURE_2D_ARRAY;
}

inline bool Is3DTexture( const TextureType texType ) noexcept
{
    return texType == TextureType::TEXTURE_3D;
}


inline bool IsCubeTexture(const TextureType texType) noexcept
{
    return texType == TextureType::TEXTURE_CUBE_MAP ||
           texType == TextureType::TEXTURE_CUBE_ARRAY;
}

inline bool IsArrayTexture(const TextureType texType) noexcept
{
    return texType == TextureType::TEXTURE_1D_ARRAY ||
           texType == TextureType::TEXTURE_2D_ARRAY ||
           texType == TextureType::TEXTURE_CUBE_ARRAY;
}

inline bool IsDepthTexture(const GFXImagePacking packing) noexcept
{
    return packing == GFXImagePacking::DEPTH ||
           packing == GFXImagePacking::DEPTH_STENCIL;
}

inline bool IsNormalizedTexture( GFXImagePacking packing ) noexcept
{
    return packing != GFXImagePacking::UNNORMALIZED;
}

inline bool SupportsZOffsetTexture( const TextureType texType ) noexcept
{
    return IsCubeTexture( texType ) || Is3DTexture( texType ) || IsArrayTexture( texType );
}

inline U8 NumChannels(const GFXImageFormat format) noexcept 
{
    switch (format)
    {
        case GFXImageFormat::RED:
        case GFXImageFormat::BC4s:
        case GFXImageFormat::BC4u: return 1u;

        case GFXImageFormat::RG:
        case GFXImageFormat::BC5s:
        case GFXImageFormat::BC5u: return 2u;

        case GFXImageFormat::BGR:
        case GFXImageFormat::RGB:
        case GFXImageFormat::BC1:
        case GFXImageFormat::BC6s:
        case GFXImageFormat::BC6u: return 3u;

        case GFXImageFormat::BGRA:
        case GFXImageFormat::RGBA:
        case GFXImageFormat::BC1a:
        case GFXImageFormat::BC2:
        case GFXImageFormat::BC3:
        case GFXImageFormat::BC3n:
        case GFXImageFormat::BC3_RGBM:
        case GFXImageFormat::BC7: return 4u;

        default:
        case GFXImageFormat::COUNT: DIVIDE_UNEXPECTED_CALL(); break;
    }

    return 0u;
}

inline bool IsBGRTexture( GFXImageFormat format ) noexcept
{
    return format == GFXImageFormat::BGR ||
           format == GFXImageFormat::BGRA;
}

FORCE_INLINE void AddImageUsageFlag( PropertyDescriptor<Texture>& descriptor, const ImageUsage usage ) noexcept
{
    descriptor._usageMask |= (1u << to_base( usage ));
}

FORCE_INLINE void RemoveImageUsageFlag( PropertyDescriptor<Texture>& descriptor, const ImageUsage usage ) noexcept
{
    descriptor._usageMask &= ~(1u << to_base( usage ));
}

FORCE_INLINE bool HasUsageFlagSet( const PropertyDescriptor<Texture>& descriptor, const ImageUsage usage ) noexcept
{
    return descriptor._usageMask & (1u << to_base( usage ));
}

template<>
inline size_t GetHash( const PropertyDescriptor<Texture>& descriptor ) noexcept
{
    size_t hash = 1337;

    Util::Hash_combine( hash,
                        descriptor._layerCount,
                        descriptor._mipBaseLevel,
                        to_base( descriptor._mipMappingState ),
                        descriptor._msaaSamples,
                        to_U32( descriptor._dataType ),
                        to_U32( descriptor._baseFormat ),
                        to_U32( descriptor._packing ),
                        to_U32( descriptor._texType ),
                        descriptor._usageMask,
                        descriptor._textureOptions._alphaChannelTransparency,
                        descriptor._textureOptions._fastCompression,
                        descriptor._textureOptions._isNormalMap,
                        descriptor._textureOptions._mipFilter,
                        descriptor._textureOptions._skipMipMaps,
                        descriptor._textureOptions._outputFormat,
                        descriptor._textureOptions._outputSRGB,
                        descriptor._textureOptions._useDDSCache
                      );

    return hash;
}
} //namespace Divide

#endif //DVD_TEXTURE_DESCRIPTOR_INL_
