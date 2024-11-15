

#include "Headers/ParticleRoundGenerator.h"

namespace Divide {

void ParticleRoundGenerator::generate([[maybe_unused]] Task& packagedTasksParent,
                                      [[maybe_unused]] TaskPool& parentPool,
                                      [[maybe_unused]] const U64 deltaTimeUS,
                                      ParticleData& p,
                                      const U32 startIndex,
                                      const U32 endIndex)
{
    const vec3<F32> center(_center + _sourcePosition);

    for (U32 i = startIndex; i < endIndex; i++)
    {
        const F32 ang = Random(0.0f, M_PI_MUL_2_f);
        p._position[i].xyz = center + vec3<F32>(_radX * std::sin(ang),
                                                _radY * std::cos(ang), 
                                                0.0f);
    }
}

} //namespace Divide
