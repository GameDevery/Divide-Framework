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

#ifndef _PARTICLE_ATTRACTOR_UPDATER_H_
#define _PARTICLE_ATTRACTOR_UPDATER_H_

#include "Dynamics/Entities/Particles/Headers/ParticleUpdater.h"

namespace Divide {

class ParticleAttractorUpdater final : public ParticleUpdater {
   public:
    /// w = force
    vectorSTD<vec4<F32>> _attractors;

   public:
    ParticleAttractorUpdater(GFXDevice& context) : ParticleUpdater(context)
    {
    }

    ~ParticleAttractorUpdater()
    {
    }

    void update(const U64 deltaTimeUS, ParticleData& p) override;

    inline size_t collectionSize() const { return _attractors.size(); }
    inline void add(const vec4<F32>& attractor) {
        _attractors.push_back(attractor);
    }
    inline vec4<F32>& get(U32 id) { return _attractors[id]; }
};
};
#endif