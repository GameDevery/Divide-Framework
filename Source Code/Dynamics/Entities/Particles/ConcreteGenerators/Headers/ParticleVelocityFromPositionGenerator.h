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

#ifndef _PARTICLE_VELOCITY_FROM_POSITION_GENERATOR_H_
#define _PARTICLE_VELOCITY_FROM_POSITION_GENERATOR_H_

#include "Dynamics/Entities/Particles/Headers/ParticleGenerator.h"

namespace Divide {
class ParticleVelocityFromPositionGenerator final : public ParticleGenerator {
   public:
    vec3<F32> _offset;
    F32 _minScale;
    F32 _maxScale;

   public:
    ParticleVelocityFromPositionGenerator() : _minScale(0.0), _maxScale(0.0) {}

    ParticleVelocityFromPositionGenerator(const vec3<F32>& offset, F32 minScale,
                                          F32 maxScale)
        : _offset(offset), _minScale(minScale), _maxScale(maxScale) {}

    void generate(Task& packagedTasksParent,
                  const U64 deltaTimeUS,
                  ParticleData& p,
                  U32 startIndex,
                  U32 endIndex) override;
};
};

#endif