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
#ifndef _ORBIT_CAMERA_H_
#define _ORBIT_CAMERA_H_

#include "FreeFlyCamera.h"

namespace Divide {

FWD_DECLARE_MANAGED_CLASS(SceneGraphNode);

/// A camera that always looks at a given target and orbits around it.
/// It's position / direction can't be changed by user input
class OrbitCamera : public FreeFlyCamera {
  protected:
    friend class Camera;
    explicit OrbitCamera(const Str128& name,
                         const CameraType& type = Type(),
                         const vec3<F32>& eye = VECTOR3_ZERO);
  public:
    void setTarget(SceneGraphNode* sgn,
                   const vec3<F32>& offsetDirection = vec3<F32>(0, 0.75, 1.0));

    void fromCamera(const Camera& camera) override;

    inline void maxRadius(F32 radius) noexcept { _maxRadius = radius; }

    inline void minRadius(F32 radius) noexcept { _minRadius = radius; }

    inline void curRadius(F32 radius) noexcept {
        _curRadius = radius;
        CLAMP<F32>(_curRadius, _minRadius, _maxRadius);
    }

    inline F32 maxRadius() const noexcept { return _maxRadius; }
    inline F32 minRadius() const noexcept { return _minRadius; }
    inline F32 curRadius() const noexcept { return _curRadius; }

    virtual void update(const U64 deltaTimeUS);
    bool zoom(I32 zoomFactor) noexcept override;

    static constexpr CameraType Type() noexcept { return CameraType::ORBIT; }

    virtual ~OrbitCamera() {}

   protected:
    virtual bool updateViewMatrix() override;

   protected:
    F32 _maxRadius = 10.0f;
    F32 _minRadius = 0.1f;
    F32 _curRadius = 8.0f;
    F32 _currentRotationX = 0.0f;
    F32 _currentRotationY = 0.0f;
    bool _rotationDirty = true;
    vec3<F32> _offsetDir = VECTOR3_ZERO;
    vec3<F32> _cameraRotation = VECTOR3_ZERO;
    vec3<F32> _newEye = VECTOR3_ZERO;
    SceneGraphNode* _targetNode = nullptr;
};


};  // namespace Divide

#endif