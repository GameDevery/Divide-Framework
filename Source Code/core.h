/*
   Copyright (c) 2014 DIVIDE-Studio
   Copyright (c) 2009 Ionut Cava

   This file is part of DIVIDE Framework.

   Permission is hereby granted, free of charge, to any person obtaining a copy of this software
   and associated documentation files (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
   INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
   OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

#ifndef CORE_H_
#define CORE_H_

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <memory>
#include <malloc.h>
#include <time.h>

#include "Hardware/Platform/Headers/PlatformDefines.h" //For data types
#include "Hardware/Platform/Headers/SharedMutex.h"     //For multi-threading
#include "Core/Math/Headers/MathClasses.h"     //For math classes (mat3,mat4,vec2,vec3,vec4 etc)
#include "Core/Headers/ApplicationTimer.h"       //For time management
#include "Core/Headers/Console.h"              //For printing to the standard output
#include "Utility/Headers/Localization.h"      //For language parsing
#include "Utility/Headers/UnorderedMap.h"
#include "Utility/Headers/Vector.h"

namespace Divide {

inline U64 GETUSTIME() {
    return ApplicationTimer::getInstance().getElapsedTime();
}

inline D32 GETTIME() {
    return getUsToSec(GETUSTIME());
}

inline D32 GETMSTIME() {
    return getUsToMs(GETUSTIME());
}

/// The following functions force a timer update (a call to query performance timer. Use these for profiling!
inline U64 GETUSTIME(bool state) {
    return ApplicationTimer::getInstance().getElapsedTime(state);
}

inline D32 GETTIME(bool state) {
    return getUsToSec(GETUSTIME(state));
}

inline D32 GETMSTIME(bool state) {
    return getUsToMs(GETUSTIME(state));
}

#ifdef _DEBUG
#define STUBBED(x) \
do {\
    static bool seen_this = false;\
    if(!seen_this){\
        seen_this = true; \
        ERROR_FN("STUBBED: %s (%s : %d)\n", \
                 x, __FILE__, __LINE__); \
    }\
} while (0);

#else
#define STUBBED(x)
#endif

//Helper method to emulate GLSL
inline F32 fract(F32 floatValue){  return (F32)fmod(floatValue, 1.0f); }
///Packs a floating point value into the [0...255] range (thx sqrt[-1] of opengl.org forums)
inline U8 PACK_FLOAT(F32 floatValue){
    //Scale and bias
  floatValue = (floatValue + 1.0f) * 0.5f;
  return (U8)(floatValue*255.0f);
}
//Pack 3 values into 1 float
inline F32 PACK_FLOAT(U8 x, U8 y, U8 z) {
  U32 packedColor = (x << 16) | (y << 8) | z;
  F32 packedFloat = (F32) ( ((D32)packedColor) / ((D32) (1 << 24)) );
   return packedFloat;
}

//UnPack 3 values from 1 float
inline void UNPACK_FLOAT(F32 src, F32& r, F32& g, F32& b){
  r = fract(src);
  g = fract(src * 256.0f);
  b = fract(src * 65536.0f);

  //Unpack to the -1..1 range
  r = (r * 2.0f) - 1.0f;
  g = (g * 2.0f) - 1.0f;
  b = (b * 2.0f) - 1.0f;
}

enum ErrorCode {
    NO_ERR = 0,
    MISSING_SCENE_DATA = -1,
    MISSING_SCENE_LOAD_CALL = -2,
    GLFW_INIT_ERROR = -3,
    GLFW_WINDOW_INIT_ERROR = -4,
    GLEW_INIT_ERROR = -5,
    GLEW_OLD_HARDWARE = -6,
    DX_INIT_ERROR = -7,
    DX_OLD_HARDWARE = -8,
    SDL_AUDIO_INIT_ERROR = -9,
    SDL_AUDIO_MIX_INIT_ERROR = -10,
    FMOD_AUDIO_INIT_ERROR = -11,
    OAL_INIT_ERROR = -12,
    PHYSX_INIT_ERROR = -13,
    PHYSX_EXTENSION_ERROR = -14,
    NO_LANGUAGE_INI = -15
};

inline const char* getErrorCodeName(ErrorCode code) {
    switch (code) {
        default:
        case NO_ERR : {
            return "Unknown error";
        };
        case MISSING_SCENE_DATA : {
            return "Invalid Scene Data. SceneManager failed to load the specified scene";
        };
        case MISSING_SCENE_LOAD_CALL : {
            return "The specified scene failed to load all of its data properly";
        };
        case GLFW_INIT_ERROR : {
            return "GLFW system failed to initialize";
        };
        case GLFW_WINDOW_INIT_ERROR : {
            return "GLFW failed to create a valid window";
        };
        case GLEW_INIT_ERROR : {
            return "GLEW failed to initialize";
        };
        case GLEW_OLD_HARDWARE : {
            return "Current hardware does not support the minimum OpenGL features required";
        };
        case DX_INIT_ERROR : {
            return "DirectX API failed to initialize";
        };
        case DX_OLD_HARDWARE : {
            return "Current hardware does not support the minimum DirectX features required";
        };
        case SDL_AUDIO_INIT_ERROR : {
            return "SDL Audio library failed to initialize";
        };
        case SDL_AUDIO_MIX_INIT_ERROR : {
            return "SDL Audio Mixer failed to initialize";
        };
        case FMOD_AUDIO_INIT_ERROR : {
            return "FMod Audio library failed to initialize";
        };
        case OAL_INIT_ERROR : {
            return "OpenAL failed to initialize";
        };
        case PHYSX_INIT_ERROR : {
            return "The PhysX library failed to initialize";
        };
        case PHYSX_EXTENSION_ERROR: {
            return "The PhysX library failed to load the required extensions";
        };
        case NO_LANGUAGE_INI : {
            return "Invalid language file";
        };
    };
}

namespace DefaultColors {
    ///Random stuff added for convenience
    inline vec4<F32> WHITE() { 
        return vec4<F32>(1.0f,1.0f,1.0f,1.0f); 
    }

    inline vec4<F32> BLACK() { 
        return vec4<F32>(0.0f,0.0f,0.0f,1.0f);
    }

    inline vec4<F32> RED()   { 
        return vec4<F32>(1.0f,0.0f,0.0f,1.0f);
    }

    inline vec4<F32> GREEN() { 
        return vec4<F32>(0.0f,1.0f,0.0f,1.0f); 
    }

    inline vec4<F32> BLUE()  { 
        return vec4<F32>(0.0f,0.0f,1.0f,1.0f);
    }

    inline vec4<F32> DIVIDE_BLUE() { 
        return vec4<F32>(0.1f,0.1f,0.8f,1.0f);
    }
};

}; //namespace Divide

#endif
