#include "Headers/ParticleEulerUpdater.h"
#include "Core/Headers/TaskPool.h"

namespace Divide {

namespace {
    const U32 g_partitionSize = 256;
};

void ParticleEulerUpdater::update(const U64 deltaTime, ParticleData& p) {
    F32 const dt = Time::MicrosecondsToSeconds<F32>(deltaTime);
    const vec4<F32> globalA(dt * _globalAcceleration, 0.0f);

    const U32 endID = p.aliveCount();

    auto parseRange = [&p, dt, globalA](const Task& parentTask, U32 start, U32 end) -> void {
        vectorImplBest<vec4<F32>>& acceleration = p._acceleration;
        for (U32 i = start; i < end; ++i) {
            vec4<F32>& acc = acceleration[i];
            acc.xyz(acc + globalA);
        }
        vectorImplBest<vec4<F32>>& velocity = p._velocity;
        for (U32 i = start; i < end; ++i) {
            vec4<F32>& vel = velocity[i];
            vel.xyz(vel + (dt * acceleration[i]));
        }

        vectorImplBest<vec4<F32>>& position = p._position;
        for (U32 i = start; i < end; ++i) {
            vec4<F32>& pos = position[i];
            pos.xyz(pos + (dt * velocity[i]));
        }
    };

    parallel_for(parseRange, endID, g_partitionSize);
}

};