#include "stdafx.h"

#include "Headers/PhysicsAsset.h"

#include "Core/Headers/PlatformContext.h"
#include "ECS/Components/Headers/RigidBodyComponent.h"

namespace Divide {
PhysicsAsset::PhysicsAsset(RigidBodyComponent& parent)
    : _parentComponent(parent),
      _context(parent.getSGN()->context().pfx())
{
}

void PhysicsAsset::physicsCollisionGroup(const PhysicsGroup group) {
    ACKNOWLEDGE_UNUSED(group);
}

}; //namespace Divide