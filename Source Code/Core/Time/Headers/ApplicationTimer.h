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

#pragma once
#ifndef _CORE_TIME_APPLICATION_TIMER_H_
#define _CORE_TIME_APPLICATION_TIMER_H_

#include "FrameRateHandler.h"

// Code from http://www.gamedev.net/reference/articles/article1382.asp
// Copyright: "Frame Rate Independent Movement" by Ben Dilts
namespace Divide {
namespace Time {
  
using TimeValue = std::chrono::time_point<std::chrono::high_resolution_clock>;
using NSec = std::chrono::nanoseconds;
using USec = std::chrono::microseconds;
using MSec = std::chrono::milliseconds;
using Sec = std::chrono::seconds;

class ApplicationTimer {


  public:
    ApplicationTimer() noexcept;
    ~ApplicationTimer() = default;

    void update();
    void reset();

    void resetFPSCounter() noexcept;
    F32 getFps() const noexcept;
    F32 getFrameTime() const noexcept;
    void getFrameRateAndTime(F32& fpsOut, F32& frameTimeOut) const noexcept;

    PROPERTY_R(F32, speedfactor, 1.0f);
    PROPERTY_R(U32, targetFrameRate, Config::TARGET_FRAME_RATE);
    PROPERTY_R(stringImpl, benchmarkReport, "");

  private:
    FrameRateHandler _frameRateHandler;
    // Measure average FPS and output max/min/average fps to console
    U64 _lastBenchmarkTimeStamp = 0UL;
};

namespace Game {
    /// The following functions return the time updated in the main app loop only!
    U64 ElapsedNanoseconds();
    U64 ElapsedMicroseconds();
    D64 ElapsedMilliseconds();
    D64 ElapsedSeconds();
};

namespace App {
    /// The following functions force a timer update (a call to query performance timer).
    U64 ElapsedNanoseconds();
    U64 ElapsedMicroseconds();
    D64 ElapsedMilliseconds();
    D64 ElapsedSeconds();
};

};  // namespace Time
};  // namespace Divide

#endif  //_CORE_TIME_APPLICATION_TIMER_H_

#include "ApplicationTimer.inl"
