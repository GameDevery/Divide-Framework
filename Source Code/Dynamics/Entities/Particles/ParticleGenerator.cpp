#include "stdafx.h"

#include "Headers./ParticleGenerator.h"

namespace Divide {

void ParticleGenerator::updateTransform(const vec3<F32>& position, const Quaternion<F32>& orientation) {
    _sourcePosition.set(position);
    _sourceOrientation.set(orientation);
}

} //namespace Divide 