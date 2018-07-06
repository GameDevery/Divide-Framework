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
#ifndef _HARDWARE_VIDEO_GFX_STATE_H_
#define _HARDWARE_VIDEO_GFX_STATE_H_

#include "Core/Math/Headers/MathVectors.h"
#include "Platform/Threading/Headers/Task.h"

namespace Divide {
    
class GPUState : private NonCopyable {
   public:
    struct GPUVideoMode {
        // width x height
        vec2<U16> _resolution;
        // bits per pixel;
        U8 _bitDepth;
        // format name;
        stringImpl _formatName;
        // Max supported
        vector<U8> _refreshRate;

        bool operator==(const GPUVideoMode& other) const {
            return _resolution == other._resolution &&
                   _bitDepth == other._bitDepth &&
                   _refreshRate == other._refreshRate &&
                   _formatName.compare(other._formatName) == 0;
        }
    };

   public:
    GPUState();
    ~GPUState();

    /// register a new display mode (resolution, bitdepth, etc).
    void registerDisplayMode(U8 displayIndex, const GPUVideoMode& mode);

    inline vec_size getDisplayCount() const {
        return _supportedDisplayModes.size();
    }

    inline const vector<GPUVideoMode>& getDisplayModes(vec_size displayIndex) const {
        assert(displayIndex < _supportedDisplayModes.size());
        return _supportedDisplayModes[displayIndex];
    }

   protected:
    // Display system
    vector<vector<GPUVideoMode>> _supportedDisplayModes;
};

};  // namespace Divide

#endif