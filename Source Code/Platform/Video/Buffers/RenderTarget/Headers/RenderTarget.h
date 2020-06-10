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
#ifndef _RENDER_TARGET_H_
#define _RENDER_TARGET_H_

#include "RTDrawDescriptor.h"
#include "RTAttachmentPool.h"
#include "Platform/Video/Headers/GraphicsResource.h"

namespace Divide {
class GFXRTPool;

struct RenderTargetID {
    RenderTargetID() noexcept : RenderTargetID(RenderTargetUsage::COUNT)
    {
    }

    RenderTargetID(const RenderTargetUsage usage) noexcept : RenderTargetID(usage, 0)
    {
    }

    RenderTargetID(const RenderTargetUsage usage, const U16 index) noexcept
        : _index(index),
          _usage(usage)
    {
    }

    U16 _index = 0;
    RenderTargetUsage _usage = RenderTargetUsage::COUNT;

    bool operator==(const RenderTargetID& other) const noexcept {
        return _index == other._index &&
               _usage == other._usage;
    }

    bool operator!=(const RenderTargetID& other) const noexcept {
        return _index != other._index ||
               _usage != other._usage;
    }
};

constexpr I16 INVALID_COLOUR_LAYER = std::numeric_limits<I16>::min();

struct BlitIndex {
    I16 _layer = INVALID_COLOUR_LAYER;
    I16 _index = INVALID_COLOUR_LAYER;
};

struct ColourBlitEntry {
    void set(const U16 indexIn, const U16 indexOut, const U16 layerIn = 0u, const U16 layerOut = 0u) {
        input(indexIn, layerIn);
        output(indexOut, layerOut);
    }

    void input(const U16 index, const U16 layer = 0u) noexcept {
        _input = { to_I16(layer), to_I16(index) };
    }

    void output(const U16 index, const U16 layer = 0u) noexcept {
        _output = { to_I16(layer), to_I16(index) };
    }

    [[nodiscard]] BlitIndex input()  const noexcept { return _input; }
    [[nodiscard]] BlitIndex output() const noexcept { return _output; }
    [[nodiscard]] bool      valid()  const noexcept { return _input._index != INVALID_COLOUR_LAYER || _input._layer != INVALID_COLOUR_LAYER; }

protected:
    BlitIndex _input;
    BlitIndex _output;
};

constexpr U16 INVALID_DEPTH_LAYER = std::numeric_limits<U16>::max();

struct DepthBlitEntry {
    U16 _inputLayer = INVALID_DEPTH_LAYER;
    U16 _outputLayer = INVALID_DEPTH_LAYER;
};

class RenderTarget;
struct RenderTargetHandle {
    RenderTargetHandle() noexcept
        : RenderTargetHandle(RenderTargetID(), nullptr)
    {
    }

    RenderTargetHandle(const RenderTargetID targetID, RenderTarget* rt) noexcept
        : _rt(rt),
        _targetID(targetID)
    {
    }

    RenderTarget* _rt;
    RenderTargetID _targetID;
};

struct RenderTargetDescriptor {
    Str64 _name = "";
    RTAttachmentDescriptor* _attachments = nullptr;
    ExternalRTAttachmentDescriptor* _externalAttachments = nullptr;
    vec2<F32> _depthRange = vec2<F32>(0.f, 1.f);
    vec2<U16>  _resolution = vec2<U16>(1u);
    F32 _depthValue = 1.0f;
    U8 _externalAttachmentCount = 0u;
    U8 _attachmentCount = 0u;
    U8 _msaaSamples = 0u;
};

class NOINITVTABLE RenderTarget : public GUIDWrapper, public GraphicsResource {
   public:
    enum class RenderTargetUsage : U8 {
        RT_READ_WRITE = 0,
        RT_READ_ONLY = 1,
        RT_WRITE_ONLY = 2
    };

    struct DrawLayerParams {
        RTAttachmentType _type = RTAttachmentType::COUNT;
        U8 _index = 0;
        U16 _layer = 0;
        bool _includeDepth = true;
    };

    struct RTBlitParams {
        RenderTarget* _inputFB = nullptr;
        DepthBlitEntry _blitDepth;
        std::array<ColourBlitEntry, RT_MAX_COLOUR_ATTACHMENTS> _blitColours;

        [[nodiscard]] bool hasBlitColours() const noexcept {
            return std::any_of(std::cbegin(_blitColours),
                               std::cend(_blitColours),
                               [](const auto& entry) {
                                    return entry.valid();
                               });
        }
    };

   protected:
    explicit RenderTarget(GFXDevice& context, const RenderTargetDescriptor& descriptor);

   public:
    virtual ~RenderTarget();

    /// Init all attachments. Returns false if already called
    virtual bool create();
    virtual void destroy();

    virtual bool hasAttachment(RTAttachmentType type, U8 index) const;
    virtual const RTAttachment_ptr& getAttachmentPtr(RTAttachmentType type, U8 index) const;
    virtual const RTAttachment& getAttachment(RTAttachmentType type, U8 index) const;
    virtual RTAttachment& getAttachment(RTAttachmentType type, U8 index);
    virtual U8 getAttachmentCount(RTAttachmentType type) const;

    virtual void clear(const RTClearDescriptor& descriptor) = 0;
    virtual void setDefaultState(const RTDrawDescriptor& drawPolicy) = 0;
    virtual void readData(const vec4<U16>& rect, GFXImageFormat imageFormat, GFXDataFormat dataType, bufferPtr outData) const = 0;
    virtual void blitFrom(const RTBlitParams& params) = 0;

    /// Resize all attachments
    bool resize(U16 width, U16 height);
    /// Change msaa sampel count for all attachments
    bool updateSampleCount(U8 newSampleCount);

    void readData(GFXImageFormat imageFormat, GFXDataFormat dataType, bufferPtr outData) const;

    U16 getWidth()  const;
    U16 getHeight() const;
    vec2<U16> getResolution() const;

    F32& depthClearValue();

    const Str64& name() const;

   protected:
    U8 _colourAttachmentCount = 0;
    RenderTargetDescriptor _descriptor;
    eastl::unique_ptr<RTAttachmentPool> _attachmentPool;
};

};  // namespace Divide


#endif //_RENDER_TARGET_H_
