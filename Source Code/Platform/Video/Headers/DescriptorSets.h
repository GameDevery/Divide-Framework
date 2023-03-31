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
#ifndef _DESCRIPTOR_SETS_H_
#define _DESCRIPTOR_SETS_H_

#include "TextureData.h"
#include "Core/Headers/Hashable.h"
#include "Platform/Video/Buffers/VertexBuffer/Headers/BufferRange.h"

namespace Divide
{
    class Texture;
    class ShaderBuffer;

    enum class DescriptorSetBindingType : U8
    {
        COMBINED_IMAGE_SAMPLER,
        IMAGE,
        UNIFORM_BUFFER,
        SHADER_STORAGE_BUFFER,
        COUNT
    };

    struct ImageSubRange
    {
        vec2<U16> _mipLevels{ 0u, U16_MAX };  //Offset, Count
        vec2<U16> _layerRange{ 0u, U16_MAX }; //Offset, Count
    };

    struct ImageView final : Hashable
    {
        struct Descriptor final : Hashable
        {
            U8 _msaaSamples{ 0u };
            GFXDataFormat _dataType{ GFXDataFormat::COUNT };
            GFXImageFormat _baseFormat{ GFXImageFormat::COUNT };
            GFXImagePacking _packing{ GFXImagePacking::COUNT };

            [[nodiscard]] size_t getHash() const noexcept override;

        } _descriptor;

        const Texture* _srcTexture{nullptr};
        ImageSubRange _subRange{};

        [[nodiscard]] size_t getHash() const noexcept override;

        [[nodiscard]] TextureType targetType() const noexcept;
        void targetType( TextureType type ) noexcept;

        private:
        friend class Texture;
        TextureType _targetType{ TextureType::COUNT };
    };

    struct DescriptorCombinedImageSampler
    {
        ImageView _image{};
        size_t _samplerHash{ 0u };
    };

    struct DescriptorImageView
    {
        ImageView _image{};
        ImageUsage _usage{ImageUsage::COUNT};
    };

    struct ShaderBufferEntry
    {
        ShaderBuffer* _buffer{ nullptr };
        BufferRange _range{};
        I32 _bufferQueueReadIndex{ 0u };
    };

    using DescriptorSetBindingData = eastl::variant<eastl::monostate, ShaderBufferEntry, DescriptorCombinedImageSampler, DescriptorImageView>;

    void Set( DescriptorSetBindingData& dataInOut, ShaderBuffer* buffer, BufferRange range ) noexcept;
    void Set( DescriptorSetBindingData& dataInOut, const DescriptorImageView& view ) noexcept;
    void Set( DescriptorSetBindingData& dataInOut, const DescriptorCombinedImageSampler& combinedImageSampler ) noexcept;
    void Set( DescriptorSetBindingData& dataInOut, const ImageView& view, ImageUsage usage) noexcept;
    void Set( DescriptorSetBindingData& dataInOut, const ImageView& view, size_t samplerHash ) noexcept;

    struct DescriptorSetBinding
    {
        DescriptorSetBindingData _data{};
        U16 _shaderStageVisibility{ to_base( ShaderStageVisibility::COUNT ) };
        U8 _slot{ 0u };
    };
    using DescriptorSet = eastl::fixed_vector<DescriptorSetBinding, 16, false>;

    [[nodiscard]] DescriptorSetBinding& AddBinding(DescriptorSet& setInOut, U8 slot, U16 stageVisibilityMask); 
    [[nodiscard]] DescriptorSetBinding& AddBinding(DescriptorSet& setInOut, U8 slot, ShaderStageVisibility stageVisibility);

    bool operator==( const ImageView& lhs, const ImageView& rhs ) noexcept;
    bool operator!=( const ImageView& lhs, const ImageView& rhs ) noexcept;
    bool operator==( const ImageSubRange& lhs, const ImageSubRange& rhs ) noexcept;
    bool operator!=( const ImageSubRange& lhs, const ImageSubRange& rhs ) noexcept;
    bool operator==( const ImageView::Descriptor& lhs, const ImageView::Descriptor& rhs ) noexcept;
    bool operator!=( const ImageView::Descriptor& lhs, const ImageView::Descriptor& rhs ) noexcept;
    bool operator==( const ShaderBufferEntry& lhs, const ShaderBufferEntry& rhs ) noexcept;
    bool operator!=( const ShaderBufferEntry& lhs, const ShaderBufferEntry& rhs ) noexcept;
    bool operator==( const DescriptorCombinedImageSampler& lhs, const DescriptorCombinedImageSampler& rhs ) noexcept;
    bool operator!=( const DescriptorCombinedImageSampler& lhs, const DescriptorCombinedImageSampler& rhs ) noexcept;
    bool operator==( const DescriptorImageView& lhs, const DescriptorImageView& rhs ) noexcept;
    bool operator!=( const DescriptorImageView& lhs, const DescriptorImageView& rhs ) noexcept;
    bool operator==( const DescriptorSetBinding& lhs, const DescriptorSetBinding& rhs ) noexcept;
    bool operator!=( const DescriptorSetBinding& lhs, const DescriptorSetBinding& rhs ) noexcept;

    [[nodiscard]] bool IsSet( const DescriptorSetBindingData& data ) noexcept;
    template<typename T>
    [[nodiscard]] T& As( DescriptorSetBindingData& data ) noexcept;
    template<typename T>
    [[nodiscard]] const T& As( const DescriptorSetBindingData& data ) noexcept;
    template<typename T>
    [[nodiscard]] bool Has( const DescriptorSetBindingData& data ) noexcept;
    [[nodiscard]] DescriptorSetBindingType Type( const DescriptorSetBindingData& data ) noexcept;

}; //namespace Divide

#endif //_DESCRIPTOR_SETS_H_

#include "DescriptorSets.inl"
