/*
Copyright (c) 2016 DIVIDE-Studio
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

#ifndef _TRANSFORM_INTERFACE_H_
#define _TRANSFORM_INTERFACE_H_

#include "Quaternion.h"

namespace Divide {
class TransformInterface {
public:
    /// Set the local X,Y and Z position
    virtual void setPosition(const vec3<F32>& position) = 0;
    /// Set the object's position on the X axis
    virtual void setPositionX(const F32 positionX) = 0;
    /// Set the object's position on the Y axis
    virtual void setPositionY(const F32 positionY) = 0;
    /// Set the object's position on the Z axis
    virtual void setPositionZ(const F32 positionZ) = 0;
    /// Add the specified translation factors to the current local position
    virtual void translate(const vec3<F32>& axisFactors) = 0;

    /// Set the local X,Y and Z scale factors
    virtual void setScale(const vec3<F32>& ammount) = 0;
    /// Set the scaling factor on the X axis
    virtual void setScaleX(const F32 ammount) = 0;
    /// Set the scaling factor on the Y axis
    virtual void setScaleY(const F32 ammount) = 0;
    /// Set the scaling factor on the Z axis
    virtual void setScaleZ(const F32 ammount) = 0;
    /// Add the specified scale factors to the current local position
    virtual void scale(const vec3<F32>& axisFactors) = 0;
    /// Increase the scaling factor on the X axis by the specified factor
    virtual void scaleX(const F32 ammount) = 0;
    /// Increase the scaling factor on the Y axis by the specified factor
    virtual void scaleY(const F32 ammount) = 0;
    /// Increase the scaling factor on the Z axis by the specified factor
    virtual void scaleZ(const F32 ammount) = 0;

    /// Set the local orientation using the Axis-Angle system.
    /// The angle can be in either degrees(default) or radians
    virtual void setRotation(const vec3<F32>& axis, F32 degrees, bool inDegrees = true) = 0;
    /// Set the local orientation using the Euler system.
    /// The angles can be in either degrees(default) or radians
    virtual void setRotation(F32 pitch, F32 yaw, F32 roll, bool inDegrees = true) = 0;
    /// Set the local orientation so that it matches the specified quaternion.
    virtual void setRotation(const Quaternion<F32>& quat) = 0;
    /// Set the rotation on the X axis (Axis-Angle used) by the specified angle
    /// (either degrees or radians)
    virtual void setRotationX(const F32 angle, bool inDegrees = true) = 0;
    /// Set the rotation on the Y axis (Axis-Angle used) by the specified angle
    /// (either degrees or radians)
    virtual void setRotationY(const F32 angle, bool inDegrees = true) = 0;
    /// Set the rotation on the Z axis (Axis-Angle used) by the specified angle
    /// (either degrees or radians)
    virtual void setRotationZ(const F32 angle, bool inDegrees = true) = 0;
    /// Apply the specified Axis-Angle rotation starting from the current orientation.
    /// The angles can be in either degrees(default) or radians
    virtual void rotate(const vec3<F32>& axis, F32 degrees, bool inDegrees = true) = 0;
    /// Apply the specified Euler rotation starting from the current orientation.
    /// The angles can be in either degrees(default) or radians
    virtual void rotate(F32 pitch, F32 yaw, F32 roll, bool inDegrees = true) = 0;
    /// Apply the specified Quaternion rotation starting from the current orientation.
    virtual void rotate(const Quaternion<F32>& quat) = 0;
    /// Perform a SLERP rotation towards the specified quaternion
    virtual void rotateSlerp(const Quaternion<F32>& quat, const D64 deltaTime) = 0;
    /// Rotate on the X axis (Axis-Angle used) by the specified angle (either degrees or radians)
    virtual void rotateX(const F32 angle, bool inDegrees = true) = 0;
    /// Rotate on the Y axis (Axis-Angle used) by the specified angle (either degrees or radians)
    virtual void rotateY(const F32 angle, bool inDegrees = true) = 0;
    /// Rotate on the Z axis (Axis-Angle used) by the specified angle (either degrees or radians)
    virtual void rotateZ(const F32 angle, bool inDegrees = true) = 0;

    inline void setPosition(F32 x, F32 y, F32 z) {
        setPosition(vec3<F32>(x, y, z));
    }

    /// Set an uniform scale on all three axis
    inline void setScale(F32 ammount) {
        setScale(vec3<F32>(ammount));
    }

    inline void setScale(F32 x, F32 y, F32 z) {
        setScale(vec3<F32>(x, y, z));
    }

    inline void setRotation(const vec3<F32>& euler, bool inDegrees = true) {
        setRotation(euler.pitch, euler.yaw, euler.roll, inDegrees);
    }

    inline void translate(F32 x, F32 y, F32 z) {
        translate(vec3<F32>(x, y, z));
    }
    
    /// Translate the object on the X axis by the specified amount
    inline void translateX(const F32 positionX) {
        translate(vec3<F32>(positionX, 0.0f, 0.0f));
    }

    /// Translate the object on the Y axis by the specified amount
    inline void translateY(const F32 positionY) {
        translate(vec3<F32>(0.0f, positionY, 0.0f));
    }

    /// Translate the object on the Z axis by the specified amount
    inline void translateZ(const F32 positionZ) {
        translate(vec3<F32>(0.0f, 0.0f, positionZ));
    }
    /// Increase the scaling factor on all three axis by an uniform factor
    inline void scale(const F32 ammount) {
        scale(vec3<F32>(ammount));
    }

    inline void scale(F32 x, F32 y, F32 z) {
        scale(vec3<F32>(x, y, z));
    }

    inline void rotate(F32 xAxis, F32 yAxis, F32 zAxis, F32 degrees, bool inDegrees = true) {
        rotate(vec3<F32>(xAxis, yAxis, zAxis), degrees, inDegrees);
    }

    inline void rotate(const vec3<F32>& euler, bool inDegrees = true) {
        rotate(euler.pitch, euler.yaw, euler.roll, inDegrees);
    }
    
    /// Return the scale factor
    virtual vec3<F32> getScale() const = 0;
    /// Return the position
    virtual vec3<F32> getPosition() const = 0;
    /// Return the orientation quaternion
    virtual Quaternion<F32> getOrientation() const = 0;

    /// Get the local transformation matrix
    /// wasRebuilt is set to true if the matrix was just rebuilt
    virtual const mat4<F32>& getMatrix() = 0;
};
};
#endif //_TRANSFORM_INTERFACE_H_