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

#ifndef _PHYSX_ACTOR_H_
#define _PHYSX_ACTOR_H_

// PhysX includes

#include <PxPhysicsAPI.h>
// Connecting the SDK to Visual Debugger
#include <pvd/PxVisualDebugger.h>
#include <extensions/PxDefaultErrorCallback.h>
#include <extensions/PxDefaultAllocator.h>
#include <extensions/PxVisualDebuggerExt.h>
#include <foundation/PxAllocatorCallback.h>
// PhysX includes //

#include "Physics/Headers/PhysicsAsset.h"

namespace Divide {

class PhysXActor : public PhysicsAsset {
public:
    PhysXActor(PhysicsComponent& parent);
    ~PhysXActor();

    void setPosition(const vec3<F32>& position) override;
    void setPositionX(const F32 positionX) override;
    void setPositionY(const F32 positionY) override;
    void setPositionZ(const F32 positionZ) override;
    void translate(const vec3<F32>& axisFactors) override;
    using TransformInterface::setPosition;

    void setScale(const vec3<F32>& ammount) override;
    void setScaleX(const F32 ammount) override;
    void setScaleY(const F32 ammount) override;
    void setScaleZ(const F32 ammount) override;
    void scale(const vec3<F32>& axisFactors) override;
    void scaleX(const F32 ammount) override;
    void scaleY(const F32 ammount) override;
    void scaleZ(const F32 ammount) override;
    using TransformInterface::setScale;

    void setRotation(const vec3<F32>& axis, F32 degrees, bool inDegrees = true) override;
    void setRotation(F32 pitch, F32 yaw, F32 roll, bool inDegrees = true) override;
    void setRotation(const Quaternion<F32>& quat) override;
    void setRotationX(const F32 angle, bool inDegrees = true) override;
    void setRotationY(const F32 angle, bool inDegrees = true) override;
    void setRotationZ(const F32 angle, bool inDegrees = true) override;
    using TransformInterface::setRotation;

    void rotate(const vec3<F32>& axis, F32 degrees, bool inDegrees = true) override;
    void rotate(F32 pitch, F32 yaw, F32 roll, bool inDegrees = true) override;
    void rotate(const Quaternion<F32>& quat) override;
    void rotateSlerp(const Quaternion<F32>& quat, const D64 deltaTime) override;
    void rotateX(const F32 angle, bool inDegrees = true) override;
    void rotateY(const F32 angle, bool inDegrees = true) override;
    void rotateZ(const F32 angle, bool inDegrees = true) override;
    using TransformInterface::rotate;

    vec3<F32> getScale() const override;
    vec3<F32> getPosition() const override;
    Quaternion<F32> getOrientation() const override;

    /// Get the local transformation matrix
    /// wasRebuilt is set to true if the matrix was just rebuilt
    const mat4<F32>& getMatrix() override;

protected:
    friend class PhysX;
    friend class PhysXSceneInterface;
    physx::PxRigidActor* _actor;
    physx::PxGeometryType::Enum _type;
    stringImpl _actorName;
    bool _isDynamic;
    F32 _userData;

    mat4<F32> _cachedLocalMatrix;
};
}; //namespace Divide

#endif //_PHYSX_ACTOR_H_