

#include "Headers/PhysXActor.h"

#include "ECS/Components/Headers/RigidBodyComponent.h"
#include "Physics/Headers/PXDevice.h"

namespace Divide {
namespace Util {
    vec3<F32> toVec3(const physx::PxVec3& vec) noexcept {
        return { vec.x, vec.y, vec.z };
    }

    physx::PxVec3 toVec3(const vec3<F32>& vec) noexcept {
        return { vec.x, vec.y, vec.z };
    }
} //namespace Util

    PhysXActor::PhysXActor(RigidBodyComponent& parent) noexcept
        : PhysicsAsset(parent)
    {
    }

    PhysXActor::~PhysXActor()
    {
        if (_actor != nullptr) {
            _actor->release();
        }
    }

    /// Set the local X,Y and Z position
    void PhysXActor::setPosition(const vec3<F32>& position) {
        setPosition(position.x, position.y, position.z);
    }

    /// Set the local X,Y and Z position
    void PhysXActor::setPosition(const F32 x, const F32 y, const F32 z) {
        DIVIDE_UNUSED(x);
        DIVIDE_UNUSED(y);
        DIVIDE_UNUSED(z);
    }

    /// Set the object's position on the X axis
    void PhysXActor::setPositionX(const F32 positionX) {
        DIVIDE_UNUSED(positionX);

    }
    /// Set the object's position on the Y axis
    void PhysXActor::setPositionY(const F32 positionY) {
        DIVIDE_UNUSED(positionY);

    }
    /// Set the object's position on the Z axis
    void PhysXActor::setPositionZ(const F32 positionZ) {
        DIVIDE_UNUSED(positionZ);
    }

    /// Set the local X,Y and Z scale factors
    void PhysXActor::setScale(const vec3<F32>& scale) {
        DIVIDE_UNUSED(scale);
    }

    /// Set the local orientation using the Axis-Angle system.
    /// The angle can be in either degrees(default) or radians
    void PhysXActor::setRotation(const vec3<F32>& axis, Angle::DEGREES<F32> degrees) {
        DIVIDE_UNUSED(axis);
        DIVIDE_UNUSED(degrees);
    }

    /// Set the local orientation using the Euler system.
    /// The angles can be in either degrees(default) or radians
    void PhysXActor::setRotation(Angle::DEGREES<F32> pitch, Angle::DEGREES<F32> yaw, Angle::DEGREES<F32> roll) {
        DIVIDE_UNUSED(pitch);
        DIVIDE_UNUSED(yaw);
        DIVIDE_UNUSED(roll);
    }

    /// Set the local orientation so that it matches the specified quaternion.
    void PhysXActor::setRotation(const Quaternion<F32>& quat) {
        DIVIDE_UNUSED(quat);
    }

    /// Add the specified translation factors to the current local position
    void PhysXActor::translate(const vec3<F32>& axisFactors) {
        DIVIDE_UNUSED(axisFactors);
    }

    /// Add the specified scale factors to the current local position
    void PhysXActor::scale(const vec3<F32>& axisFactors) {
        DIVIDE_UNUSED(axisFactors);
    }

    /// Apply the specified Axis-Angle rotation starting from the current
    /// orientation.
    /// The angles can be in either degrees(default) or radians
    void PhysXActor::rotate(const vec3<F32>& axis, Angle::DEGREES<F32> degrees) {
        DIVIDE_UNUSED(axis);
        DIVIDE_UNUSED(degrees);
    }

    /// Apply the specified Euler rotation starting from the current
    /// orientation.
    /// The angles can be in either degrees(default) or radians
    void PhysXActor::rotate(Angle::DEGREES<F32> pitch, Angle::DEGREES<F32> yaw, Angle::DEGREES<F32> roll) {
        DIVIDE_UNUSED(pitch);
        DIVIDE_UNUSED(yaw);
        DIVIDE_UNUSED(roll);
    }

    /// Apply the specified Quaternion rotation starting from the current orientation.
    void PhysXActor::rotate(const Quaternion<F32>& quat) {
        DIVIDE_UNUSED(quat);
    }

    /// Perform a SLERP rotation towards the specified quaternion
    void PhysXActor::rotateSlerp(const Quaternion<F32>& quat, const D64 deltaTime) {
        DIVIDE_UNUSED(quat);
        DIVIDE_UNUSED(deltaTime);
    }

    /// Set the scaling factor on the X axis
    void PhysXActor::setScaleX(const F32 amount) {
        DIVIDE_UNUSED(amount);
    }

    /// Set the scaling factor on the Y axis
    void PhysXActor::setScaleY(const F32 amount) {
        DIVIDE_UNUSED(amount);
    }

    /// Set the scaling factor on the Z axis
    void PhysXActor::setScaleZ(const F32 amount) {
        DIVIDE_UNUSED(amount);
    }

    /// Increase the scaling factor on the X axis by the specified factor
    void PhysXActor::scaleX(const F32 amount) {
        DIVIDE_UNUSED( amount );
    }

    /// Increase the scaling factor on the Y axis by the specified factor
    void PhysXActor::scaleY(const F32 amount) {
        DIVIDE_UNUSED( amount );
    }

    /// Increase the scaling factor on the Z axis by the specified factor
    void PhysXActor::scaleZ(const F32 amount) {
        DIVIDE_UNUSED( amount );
    }

    /// Rotate on the X axis (Axis-Angle used) by the specified angle (either
    /// degrees or radians)
    void PhysXActor::rotateX(const Angle::DEGREES<F32> angle) {
        DIVIDE_UNUSED( angle );
    }

    /// Rotate on the Y axis (Axis-Angle used) by the specified angle (either
    /// degrees or radians)
    void PhysXActor::rotateY(const Angle::DEGREES<F32> angle) {
        DIVIDE_UNUSED( angle );
    }

    /// Rotate on the Z axis (Axis-Angle used) by the specified angle (either
    /// degrees or radians)
    void PhysXActor::rotateZ(const Angle::DEGREES<F32> angle) {
        DIVIDE_UNUSED( angle );
    }

    /// Set the rotation on the X axis (Axis-Angle used) by the specified angle
    /// (either degrees or radians)
    void PhysXActor::setRotationX(const Angle::DEGREES<F32> angle) {
        DIVIDE_UNUSED( angle );
    }

    /// Set the rotation on the Y axis (Axis-Angle used) by the specified angle
    /// (either degrees or radians)
    void PhysXActor::setRotationY(const Angle::DEGREES<F32> angle) {
        DIVIDE_UNUSED( angle );
    }

    /// Set the rotation on the Z axis (Axis-Angle used) by the specified angle
    /// (either degrees or radians)
    void PhysXActor::setRotationZ(const Angle::DEGREES<F32> angle) {
        DIVIDE_UNUSED( angle );
    }
    
    /// Return the scale factor
    void PhysXActor::getScale(vec3<F32>& scaleOut) const {
        scaleOut.set(1.0f);
    }

    /// Return the position
    void PhysXActor::getPosition(vec3<F32>& posOut) const {
        posOut.set(0.0f);
    }

    /// Return the orientation quaternion
    void  PhysXActor::getOrientation(Quaternion<F32>& quatOut) const {
        quatOut.identity();
    }

    void PhysXActor::physicsCollisionGroup(const PhysicsGroup group) {
        if (_parentComponent.physicsCollisionGroup() != group && _actor != nullptr) {
            _context.convertActor(this, group);
        }

        PhysicsAsset::physicsCollisionGroup(group);
    }

} //namespace Divide
