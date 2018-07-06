#include "Headers/Unit.h"
#include "Graphs/Headers/SceneGraphNode.h"
#include "Core/Headers/ApplicationTimer.h"
#include "Core/Math/Headers/Transform.h"
#include "Managers/Headers/FrameListenerManager.h"

namespace Divide {

Unit::Unit(UnitType type, SceneGraphNode* const node) : FrameListener(),
                                                        _type(type),
                                                        _node(node),
                                                        _moveSpeed(metre(1)),
                                                        _moveTolerance(0.1f),
                                                        _prevTime(0)
{
    DIVIDE_ASSERT(node != nullptr, "Unit error: Invalid parent node specified!");
    REGISTER_FRAME_LISTENER(this, 5);
    _node->registerdeletionCallback(DELEGATE_BIND(&Unit::nodeDeleted, this));
    _currentPosition = _node->getComponent<PhysicsComponent>()->getConstTransform()->getPosition();
}

Unit::~Unit()
{
    UNREGISTER_FRAME_LISTENER(this);
}

/// Pathfinding, collision detection, animation playback should all be controlled from here
bool Unit::moveTo(const vec3<F32>& targetPosition) {
    // We should always have a node
    if(!_node) {
        return false;
    }
    WriteLock w_lock(_unitUpdateMutex);
    // We receive move request every frame for now (or every task tick)
    // Start plotting a course from our current position
    _currentPosition = _node->getComponent<PhysicsComponent>()->getConstTransform()->getPosition();
    _currentTargetPosition = targetPosition;

    if (_prevTime <= 0) {
        _prevTime = GETMSTIME();
    }
    // get current time in ms
    D32 currentTime = GETMSTIME();
    // figure out how many milliseconds have elapsed since last move time
    D32 timeDif = currentTime - _prevTime;
    CLAMP<D32>(timeDif, 0, timeDif);
    // update previous time
    _prevTime = currentTime;
    // 'moveSpeed' m/s = '0.001 * moveSpeed' m / ms
    // distance = timeDif * 0.001 * moveSpeed
    F32 moveDistance = std::min((F32)(_moveSpeed * (getMsToSec(timeDif))), 0.0f);

    bool returnValue = IS_TOLERANCE(moveDistance, centimetre(1));
    if (!returnValue) {
        F32 xDelta = _currentTargetPosition.x - _currentPosition.x;
        F32 yDelta = _currentTargetPosition.y - _currentPosition.y;
        F32 zDelta = _currentTargetPosition.z - _currentPosition.z;
        bool xTolerance = IS_TOLERANCE(xDelta, _moveTolerance);
        bool yTolerance = IS_TOLERANCE(yDelta, _moveTolerance);
        bool zTolerance = IS_TOLERANCE(zDelta, _moveTolerance);
        // apply framerate variance
#       if !USE_FIXED_TIMESTEP
            //moveDistance *= FRAME_SPEED_FACTOR;
#       endif
        // Compute the destination point for current frame step
        vec3<F32> interpPosition;
        if (!yTolerance && !IS_ZERO( yDelta ) ) {
            interpPosition.y = ( _currentPosition.y > _currentTargetPosition.y ? -moveDistance : moveDistance );
        }
        if ((!xTolerance || !zTolerance)) {
            // Update target
            if (IS_ZERO( xDelta )) {
                interpPosition.z = ( _currentPosition.z > _currentTargetPosition.z ? -moveDistance : moveDistance );
            } else if( IS_ZERO( zDelta ) ){
                interpPosition.x = ( _currentPosition.x > _currentTargetPosition.x ? -moveDistance : moveDistance );
            } else if( std::fabs( xDelta ) > std::fabs( zDelta ) ) {
                F32 value = std::fabs( zDelta / xDelta ) * moveDistance;
                interpPosition.z = ( _currentPosition.z > _currentTargetPosition.z ? -value : value );
                interpPosition.x = ( _currentPosition.x > _currentTargetPosition.x ? -moveDistance : moveDistance );
            } else {
                F32 value = std::fabs( xDelta / zDelta ) * moveDistance;
                interpPosition.x = ( _currentPosition.x > _currentTargetPosition.x ? -value : value );
                interpPosition.z = ( _currentPosition.z > _currentTargetPosition.z ? -moveDistance : moveDistance );
            }
            // commit transformations
            _node->getComponent<PhysicsComponent>()->translate(interpPosition);
        }
    }

    return returnValue;
}

/// Move along the X axis
bool Unit::moveToX(const F32 targetPosition) {
    if(!_node) {
        return false;
    }
    /// Update current position
    WriteLock w_lock(_unitUpdateMutex);
    _currentPosition = _node->getComponent<PhysicsComponent>()->getConstTransform()->getPosition();
    w_lock.unlock();
    return moveTo(vec3<F32>(targetPosition,_currentPosition.y,_currentPosition.z));
}

/// Move along the Y axis
bool Unit::moveToY(const F32 targetPosition) {
    if(!_node) {
        return false;
    }
    /// Update current position
    WriteLock w_lock(_unitUpdateMutex);
    _currentPosition = _node->getComponent<PhysicsComponent>()->getConstTransform()->getPosition();
    w_lock.unlock();
    return moveTo(vec3<F32>(_currentPosition.x,targetPosition,_currentPosition.z));
}

/// Move along the Z axis
bool Unit::moveToZ(const F32 targetPosition) {
    if(!_node) {
        return false;
    }
    /// Update current position
    WriteLock w_lock(_unitUpdateMutex);
    _currentPosition = _node->getComponent<PhysicsComponent>()->getConstTransform()->getPosition();
    w_lock.unlock();
    return moveTo(vec3<F32>(_currentPosition.x,_currentPosition.y,targetPosition));
}

/// Further improvements may imply a cooldown and collision detection at destination (thus the if-check at the end)
bool Unit::teleportTo(const vec3<F32>& targetPosition) {
    /// We should always have a node
    if(!_node) {
        return false;
    }
    WriteLock w_lock(_unitUpdateMutex);
    /// We receive move request every frame for now (or every task tick)
    /// Check if the current request is already processed
    if( !_currentTargetPosition.compare(targetPosition,0.00001f)) {
        /// Update target
        _currentTargetPosition = targetPosition;
    }
    PhysicsComponent* nodePhysicsComponent = _node->getComponent<PhysicsComponent>();
    /// Start plotting a course from our current position
    _currentPosition = nodePhysicsComponent->getConstTransform()->getPosition();
    /// teleport to desired position
    nodePhysicsComponent->setPosition(_currentTargetPosition);
    /// Update current position
    _currentPosition = nodePhysicsComponent->getConstTransform()->getPosition();
    /// And check if we arrived
    if (_currentTargetPosition.compare(_currentPosition,0.0001f)) {
        return true; ///< yes
    }

    return false; ///< no
}

};