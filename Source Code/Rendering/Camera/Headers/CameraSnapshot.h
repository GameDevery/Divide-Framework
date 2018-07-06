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
#ifndef _CAMERA_SNAPSHOT_H_
#define _CAMERA_SNAPSHOT_H_

#include "Core/Math/Headers/MathMatrices.h"

namespace Divide {
    struct CameraSnapshot {
        mat4<F32> _viewMatrix;
        mat4<F32> _projectionMatrix;
        vec3<F32> _eye;
        Angle::DEGREES<F32> _FoV = 0.0f;
        vec2<F32> _zPlanes;
        F32 _aspectRatio = 0.0f;
        F32 _padding_ = 0.0f;
    };

    bool operator==(const CameraSnapshot& lhs, const CameraSnapshot& rhs);
    bool operator!=(const CameraSnapshot& lhs, const CameraSnapshot& rhs);
};

#endif //_CAMERA_SNAPSHOT_H_