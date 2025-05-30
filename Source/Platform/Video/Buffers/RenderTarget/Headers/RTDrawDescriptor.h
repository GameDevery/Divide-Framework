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
#ifndef DVD_RENDER_TARGET_DRAW_DESCRIPTOR_H_
#define DVD_RENDER_TARGET_DRAW_DESCRIPTOR_H_

#include "RTAttachment.h"
#include "Platform/Video/Headers/BlendingProperties.h"

namespace Divide
{

constexpr U8 INVALID_INDEX = U8_MAX;
constexpr U16 MAX_BLIT_ENTRIES = 8u;

struct BlitEntry
{
    U16 _layerOffset{0u};
    U16 _mipOffset{0u};
    U8 _index{ INVALID_INDEX };

    bool operator==(const BlitEntry&) const = default;
};

struct DrawLayerEntry
{
    SubRange _layer{0u, 1u};
    /// Ignored for non cube textures
    U8 _cubeFace{0u}; 

    bool operator==(const DrawLayerEntry&) const = default;
};

struct RTClearEntry
{
    FColour4 _colour{0.f}; //For depth, only R channel is used
    bool _enabled{false};
};

struct RTBlitEntry
{
    BlitEntry _input{};
    BlitEntry _output{};
    U16 _layerCount{1u};
    U16 _mipCount{1u};
};

using RTBlitParams = eastl::fixed_vector<RTBlitEntry, MAX_BLIT_ENTRIES, false>;
using RTClearDescriptor = std::array<RTClearEntry, RT_MAX_ATTACHMENT_COUNT>;
using RTTransitionMask = bool[RT_MAX_ATTACHMENT_COUNT];
using RTDrawMask = bool[to_base( RTColourAttachmentSlot::COUNT )];

struct RTDrawDescriptor
{
    std::array<DrawLayerEntry, RT_MAX_ATTACHMENT_COUNT> _writeLayers{};
    RTDrawMask _drawMask = {false, false, false, false};
    U16 _mipWriteLevel{ 0u };
    bool _autoResolveMSAA{true};
    bool _keepMSAADataAfterResolve{ false };
};

extern BlitEntry INVALID_BLIT_ENTRY;
extern RTClearEntry DEFAULT_CLEAR_ENTRY;

bool IsValid( const RTBlitParams& params) noexcept;

}; //namespace Divide

#endif //DVD_RENDER_TARGET_DRAW_DESCRIPTOR_H_

#include "RTDrawDescriptor.inl"
