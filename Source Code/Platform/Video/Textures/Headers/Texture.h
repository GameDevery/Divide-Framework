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
#include "Platform/Video/Headers/TextureData.h"
#include "Platform/Video/Headers/DescriptorSets.h"

#include "Platform/Video/Headers/GraphicsResource.h"
#include "Platform/Video/Textures/Headers/TextureDescriptor.h"

#include "Utility/Headers/ImageTools.h"

namespace Divide
{

class Kernel;

namespace Attorney
{
    class TextureKernel;
};

namespace TypeUtil
{
    [[nodiscard]] const char* WrapModeToString( TextureWrap wrapMode ) noexcept;
    [[nodiscard]] TextureWrap StringToWrapMode( const string& wrapMode );

    [[nodiscard]] const char* TextureFilterToString( TextureFilter filter ) noexcept;
    [[nodiscard]] TextureFilter StringToTextureFilter( const string& filter );

    [[nodiscard]] const char* TextureMipSamplingToString( TextureMipSampling sampling ) noexcept;
    [[nodiscard]] TextureMipSampling StringToTextureMipSampling( const string& sampling );
};

FWD_DECLARE_MANAGED_CLASS( Texture );

struct TextureLayoutChange
{
    ImageView _targetView;
    ImageUsage _sourceLayout{ ImageUsage::COUNT };
    ImageUsage _targetLayout{ ImageUsage::COUNT };
};

using TextureLayoutChanges = eastl::fixed_vector<TextureLayoutChange, 6, true>;

[[nodiscard]] bool IsEmpty( const TextureLayoutChanges& changes ) noexcept;

/// An API-independent representation of a texture
class NOINITVTABLE Texture : public CachedResource, public GraphicsResource
{
    friend class ResourceCache;
    friend class ResourceLoader;
    template <typename T>
    friend class ImplResourceLoader;
    friend class Attorney::TextureKernel;

    public:
        explicit Texture( GFXDevice& context,
                          size_t descriptorHash,
                          const Str256& name,
                          const ResourcePath& assetNames,
                          const ResourcePath& assetLocations,
                          const TextureDescriptor& texDescriptor,
                          ResourceCache& parentCache );

        virtual ~Texture();

        static void OnStartup( GFXDevice& gfx );
        static void OnShutdown() noexcept;
        static ResourcePath GetCachePath( ResourcePath originalPath ) noexcept;
        static [[nodiscard]] bool UseTextureDDSCache() noexcept;
        static [[nodiscard]] const Texture_ptr& DefaultTexture2D() noexcept;
        static [[nodiscard]] const Texture_ptr& DefaultTexture2DArray() noexcept;
        static [[nodiscard]] const size_t DefaultSamplerHash() noexcept;
        static [[nodiscard]] U8 GetBytesPerPixel( GFXDataFormat format, GFXImageFormat baseFormat, GFXImagePacking packing ) noexcept;

        /// API-dependent loading function that uploads ptr data to the GPU using the specified parameters
        void createWithData( const ImageTools::ImageData& imageData, const PixelAlignment& pixelUnpackAlignment );
        void createWithData( const Byte* data, size_t dataSize, const vec2<U16>& dimensions, const PixelAlignment& pixelUnpackAlignment );
        void createWithData( const Byte* data, size_t dataSize, const vec3<U16>& dimensions, const PixelAlignment& pixelUnpackAlignment );

        void replaceData(const Byte* data, size_t dataSize, const vec3<U16>& offset, const vec3<U16>& range, const PixelAlignment& pixelUnpackAlignment );

        /// Change the number of MSAA samples for this current texture
        void setSampleCount( U8 newSampleCount );

        [[nodiscard]] ImageView getView() const noexcept;
        [[nodiscard]] ImageView getView( TextureType targetType ) const noexcept;
        [[nodiscard]] ImageView getView( SubRange mipRange ) const noexcept;
        [[nodiscard]] ImageView getView( SubRange mipRange, SubRange layerRange ) const noexcept;
        [[nodiscard]] ImageView getView( TextureType targetType, SubRange mipRange ) const noexcept;
        [[nodiscard]] ImageView getView( TextureType targetType, SubRange mipRange, SubRange layerRange/*offset, count*/ ) const noexcept;

        [[nodiscard]] virtual ImageReadbackData readData( U8 mipLevel, const PixelAlignment& pixelPackAlignment) const = 0;

        PROPERTY_R( TextureDescriptor, descriptor );
        /// Get the number of mips
        PROPERTY_R( U16, mipCount, 1u );
        /// Texture width as returned by STB/DDS loader
        PROPERTY_R( U16, width, 0u );
        /// Texture height as returned by STB/DDS loader
        PROPERTY_R( U16, height, 0u );
        /// Depth for TEXTURE_3D, layer count for TEXTURE_1/2D/CUBE_ARRAY. For cube arrays, numSlices = depth * 6u
        PROPERTY_R( U16, depth, 1u );
        /// If the texture has an alpha channel and at least one pixel is translucent, return true
        PROPERTY_R( bool, hasTranslucency, false );
        /// If the texture has an alpha channel and at least on pixel is fully transparent and no pixels are partially transparent, return true
        PROPERTY_R( bool, hasTransparency, false );

        [[nodiscard]] U8 numChannels() const noexcept;

     protected:
        /// Use STB to load a file into a Texture Object
        bool loadFile( const ResourcePath& path, const ResourcePath& name, ImageTools::ImageData& fileData );
        bool checkTransparency( const ResourcePath& path, const ResourcePath& name, ImageTools::ImageData& fileData );
        /// Load texture data using the specified file name
        bool load() override;
        virtual void threadedLoad();
        virtual void postLoad();
        virtual bool unload();

        void validateDescriptor();

        [[nodiscard]] const char* getResourceTypeName() const noexcept override { return "Texture"; }

        virtual void loadDataInternal( const ImageTools::ImageData& imageData, const vec3<U16>& offset, const PixelAlignment& pixelUnpackAlignment ) = 0;
        virtual void loadDataInternal( const Byte* data, size_t size, U8 targetMip, const vec3<U16>& offset, const vec3<U16>& dimensions, const PixelAlignment& pixelUnpackAlignment ) = 0;
        virtual void prepareTextureData( U16 width, U16 height, U16 depth, bool emptyAllocation );
        virtual void submitTextureData();

    protected:
        ResourceCache& _parentCache;
        TextureType _type{ TextureType::COUNT };

    protected:
        static bool s_useDDSCache;
        static size_t s_defaultSamplerHash;
        static Texture_ptr s_defaultTexture2D;
        static Texture_ptr s_defaultTexture2DArray;
        static ResourcePath s_missingTextureFileName;
};

namespace Attorney
{
    class TextureKernel
    {
        protected:
        static void UseTextureDDSCache( const bool state ) noexcept
        {
            Texture::s_useDDSCache = state;
        }

        friend class Kernel;
    };
}

};  // namespace Divide
#endif // _TEXTURE_H_
