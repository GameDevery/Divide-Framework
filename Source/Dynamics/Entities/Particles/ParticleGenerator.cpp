

#include "Headers/ParticleGenerator.h"

namespace Divide {

void ParticleGenerator::updateTransform(const float3& position, const Quaternion<F32>& orientation) noexcept {
    _sourcePosition.set(position);
    _sourceOrientation.set(orientation);
}

} //namespace Divide
