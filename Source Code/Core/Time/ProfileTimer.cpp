#include "stdafx.h"

#include "config.h"

#include "Headers/ProfileTimer.h"
#include "Headers/ApplicationTimer.h"

#include "Core/Headers/Console.h"
#include "Core/Headers/StringHelper.h"

namespace Divide {
namespace Time {

namespace {
    std::array<ProfileTimer, Config::Profile::MAX_PROFILE_TIMERS> g_profileTimers;
    std::array<bool, Config::Profile::MAX_PROFILE_TIMERS> g_profileTimersState;

    bool g_timersInit = false;
};

bool ProfileTimer::s_enabled = true;

ScopedTimer::ScopedTimer(ProfileTimer& timer) 
    : _timer(timer)
{
    _timer.start();
}

ScopedTimer::~ScopedTimer()
{
    _timer.stop();
}

ProfileTimer::ProfileTimer()
    : _timer(0UL),
      _timerAverage(0UL),
      _timerCounter(0),
      _globalIndex(0),
      _parent(Config::Profile::MAX_PROFILE_TIMERS + 1),
      _appTimer(ApplicationTimer::instance())
{
}

ProfileTimer::~ProfileTimer()
{
}

void ProfileTimer::start() {
    if (Config::Profile::ENABLE_FUNCTION_PROFILING && timersEnabled()) {
        _timer = _appTimer.getElapsedTime(true);
    }
}

U64 ProfileTimer::stop() {
    if (Config::Profile::ENABLE_FUNCTION_PROFILING && timersEnabled()) {
        _timerAverage += _appTimer.getElapsedTime(true) - _timer;
        _timerCounter++;
    }

    return get();
}

void ProfileTimer::reset() {
    _timerAverage = 0;
    _timerCounter = 0;
}

void ProfileTimer::addChildTimer(ProfileTimer& child) {
    // must not have a parent
    assert(child._parent > Config::Profile::MAX_PROFILE_TIMERS);
    // must be unique child
    assert(!hasChildTimer(child));

    _children.push_back(child._globalIndex);
    child._parent = _globalIndex;
}

void ProfileTimer::removeChildTimer(ProfileTimer& child) {
    U32 childID = child._globalIndex;

    _children.erase(
        std::remove_if(std::begin(_children),
                       std::end(_children),
                       [childID](U32 entry) {
                           return entry == childID;
                        }),
        std::end(_children));
    child._parent = Config::Profile::MAX_PROFILE_TIMERS + 1;
}

bool ProfileTimer::hasChildTimer(ProfileTimer& child) {
    U32 childID = child._globalIndex;

    return std::find_if(std::cbegin(_children),
                        std::cend(_children),
                        [childID](U32 entry) {
                            return entry == childID;
            }) != std::cend(_children);
}

U64 ProfileTimer::getChildTotal() const {
    U64 ret = 0;
    for (U32 child : _children) {
        if (g_profileTimersState[child]) {
            ret += g_profileTimers[child].get();
        }
    }
    return ret;
}

stringImpl ProfileTimer::print(U32 level) const {
    if (Config::Profile::ENABLE_FUNCTION_PROFILING) {
        stringImpl ret(Util::StringFormat("[ %s ] : [ %5.3f ms]",
                                          _name.c_str(),
                                          MicrosecondsToMilliseconds<F32>(get())));
        for (U32 child : _children) {
            if (g_profileTimersState[child]) {
                ret.append("\n    " + g_profileTimers[child].print(level + 1));
            }
        }

        for (U32 i = 0; i < level; ++i) {
            ret.insert(0, "    ");
        }

        return ret;
    }

    return "";
}

U64 ProfileTimer::overhead() {
    static const U8 overheadLoopCount = 3;
    U64 overhead = 0;
    ProfileTimer test;
    bool prevState = timersEnabled();

    if (!prevState) {
        enableTimers();
    }

    for (U8 i = 0; i < overheadLoopCount; ++i) {
        test.start();
        test.stop();
        overhead += test.get();
    }

    if (!prevState) {
        disableTimers();
    }

    return overhead / overheadLoopCount;
}

stringImpl ProfileTimer::printAll() {
    stringImpl ret(Util::StringFormat("Profiler overhead: [%d us]\n", overhead()));

    for (ProfileTimer& entry : g_profileTimers) {
        if (!g_profileTimersState[entry._globalIndex] ||
                entry._parent < Config::Profile::MAX_PROFILE_TIMERS ||
                    entry._timerCounter == 0) {
            continue;
        }
        ret.append(entry.print());
        ret.append("\n");
        entry.reset();
    }

    return ret;
}

ProfileTimer& ProfileTimer::getNewTimer(const char* timerName) {
    if (!g_timersInit) {
        g_profileTimersState.fill(false);

        U32 index = 0;
        for (ProfileTimer& entry : g_profileTimers) {
            entry._globalIndex = index++;
        }
        g_timersInit = true;
    }

    for (ProfileTimer& entry : g_profileTimers) {
        if (!g_profileTimersState[entry._globalIndex]) {
            entry.reset();
            entry._name = timerName;
            g_profileTimersState[entry._globalIndex] = true;
            return entry;
        }
    }

    DIVIDE_UNEXPECTED_CALL("Reached max profile timer count!");
    return g_profileTimers[0];
}

void ProfileTimer::removeTimer(ProfileTimer& timer) {
    g_profileTimersState[timer._globalIndex] = false;
    if (timer._parent < Config::Profile::MAX_PROFILE_TIMERS) {
        g_profileTimers[timer._parent].removeChildTimer(timer);
    }
}

bool ProfileTimer::timersEnabled() {
    return s_enabled;
}

void ProfileTimer::enableTimers() {
    s_enabled = true;
}

void ProfileTimer::disableTimers() {
    s_enabled = false;
}

};  // namespace Time
};  // namespace Divide