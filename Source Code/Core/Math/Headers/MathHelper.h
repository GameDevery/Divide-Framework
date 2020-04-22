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

/*Code references:
    Matrix inverse: http://www.devmaster.net/forums/showthread.php?t=14569
    Matrix multiply:
   http://fhtr.blogspot.com/2010/02/4x4-float-matrix-multiplication-using.html
    Square root: http://www.codeproject.com/KB/cpp/Sqrt_Prec_VS_Speed.aspx
*/

#pragma once
#pragma once
#ifndef _CORE_MATH_MATH_HELPER_H_
#define _CORE_MATH_MATH_HELPER_H_

#include "Platform/Headers/PlatformDefines.h"

namespace Divide {
template <typename T>
class mat2;
template <typename T>
class mat3;
template <typename T>
class mat4;
template <typename T>
class vec2;
template <typename T>
class vec3;
template <typename T>
class vec4;
template <typename T>
class Rect;
template <typename T>
class Quaternion;

using UColour3 = vec3<U8>;
using FColour3 = vec3<F32>;

using UColour4 = vec4<U8>;
using FColour4 = vec4<F32>;

#if !defined(M_PI)
    constexpr D64 M_PI = 3.14159265358979323846;
#endif

    constexpr D64 M_2PI = 6.28318530717958647692;
    constexpr D64 M_PIDIV180 = 0.01745329251994329576;
    constexpr D64 M_180DIVPI = 57.29577951308232087679;
    constexpr D64 M_PIDIV360 = 0.00872664625997164788;
    constexpr F32 INV_RAND_MAX = 0.0000305185094f;
    constexpr D64 M_PI2 = M_2PI;

template<typename T>
using SignedIntegerBasedOnSize = typename std::conditional<sizeof(T) == 8, I64, I32>::type;
template<typename T>
using UnsignedIntegerBasedOnSize = typename std::conditional<sizeof(T) == 8, U64, U32>::type;
template<typename T>
using IntegerTypeBasedOnSign = typename std::conditional<std::is_unsigned<T>::value,
                                                         UnsignedIntegerBasedOnSize<T>,
                                                         SignedIntegerBasedOnSize<T>>::type;

template<typename T>
using DefaultDistribution = typename std::conditional<std::is_integral<T>::value, 
                                     std::uniform_int_distribution<IntegerTypeBasedOnSign<T>>,
                                     std::uniform_real_distribution<T>>::type;

template <typename T, 
          typename Engine = std::mt19937_64,
          typename Distribution = DefaultDistribution<T>>
T Random(T min, T max) noexcept;

template <typename T,
          typename Engine = std::mt19937_64,
          typename Distribution = DefaultDistribution<T>>
T Random(T max) noexcept;

template <typename T,
          typename Engine = std::mt19937_64,
          typename Distribution = DefaultDistribution<T>>
T Random() noexcept;

template<typename Engine = std::mt19937_64>
void SeedRandom();

template<typename Engine = std::mt19937_64>
void SeedRandom(U32 seed);

template<typename Mask, typename Type>
constexpr typename std::enable_if<std::is_enum<Type>::value, bool>::type
BitCompare(const Mask bitMask, const Type bit) noexcept;

template<typename Mask, typename Type>
constexpr typename std::enable_if<std::is_enum<Type>::value, void>::type
SetBit(Mask& bitMask, const Type bit) noexcept;

template<typename Mask, typename Type>
constexpr typename std::enable_if<std::is_enum<Type>::value, void>::type
ClearBit(Mask& bitMask, const Type bit) noexcept;

template<typename Mask, typename Type>
constexpr typename std::enable_if<std::is_enum<Type>::value, void>::type
ToggleBit(Mask& bitMask, const Type bit) noexcept;

template<typename Mask, typename Type>
constexpr typename std::enable_if<std::is_enum<Type>::value, void>::type
ToggleBit(Mask& bitMask, const Type bit, bool state) noexcept;

template<typename Mask>
constexpr bool AnyCompare(const Mask bitMask, const Mask checkMask) noexcept;
template<typename Mask>
constexpr bool AllCompare(const Mask bitMask, const Mask checkMask) noexcept;
template<typename Mask>
constexpr bool BitCompare(const Mask bitMask, const Mask bit) noexcept;
template<typename Mask>
constexpr void SetBit(Mask& bitMask, const Mask bit) noexcept;
template<typename Mask>
constexpr void ClearBit(Mask& bitMask, const Mask bit) noexcept;
template<typename Mask>
constexpr void ToggleBit(Mask& bitMask, const Mask bit) noexcept;
template<typename Mask>
constexpr void ToggleBit(Mask& bitMask, const Mask bit, bool state) noexcept;

template<typename Mask, typename Type>
constexpr typename std::enable_if<std::is_enum<Type>::value, bool>::type
BitCompare(const std::atomic<Mask> bitMask, const Type bit) noexcept;

template<typename Mask, typename Type>
constexpr typename std::enable_if<std::is_enum<Type>::value, void>::type
SetBit(std::atomic<Mask>& bitMask, const Type bit) noexcept;

template<typename Mask, typename Type>
constexpr typename std::enable_if<std::is_enum<Type>::value, void>::type
ClearBit(std::atomic<Mask>& bitMask, const Type bit) noexcept;

template<typename Mask>
constexpr bool BitCompare(const std::atomic<Mask>& bitMask, const Mask bit) noexcept;
template<typename Mask>
constexpr void SetBit(std::atomic<Mask>& bitMask, const Mask bit) noexcept;
template<typename Mask>
constexpr void ClearBit(std::atomic<Mask>& bitMask, const Mask bit) noexcept;
template<typename Mask>
constexpr void ToggleBit(std::atomic<Mask>& bitMask, const Mask bit) noexcept;

/// Clamps value n between min and max
template <typename T>
void CLAMP(T& n, const T min, const T max) noexcept;

template <typename T>
T CLAMPED(const T& n, const T min, const T max) noexcept;

template <typename T>
T CLAMPED_01(const T& n) noexcept;

template <typename T>
T MAP(T input, const T in_min, const T in_max, const T out_min, const T out_max, D64& slopeOut) noexcept;

template <typename T>
T SQUARED(T input) noexcept;

template <typename T>
T SIGNED_SQUARED(T input) noexcept;

template <typename T>
T MAP(T input, const T in_min, const T in_max, const T out_min, const T out_max) noexcept {
    D64 slope = 0.0;
    return MAP(input, in_min, in_max, out_min, out_max, slope);
}

template <typename T>
vec2<T> COORD_REMAP(vec2<T> input, const Rect<T>& in_rect, const Rect<T>& out_rect) noexcept {
    return vec2<T> {
        MAP(input.x, in_rect.x, in_rect.z, out_rect.x, out_rect.z),
        MAP(input.y, in_rect.y, in_rect.w, out_rect.y, out_rect.w)
    };
}

template <typename T>
vec3<T> COORD_REMAP(vec3<T> input, const Rect<T>& in_rect, const Rect<T>& out_rect) noexcept {
    return vec3<T>(COORD_REMAP(input.xy(), in_rect, out_rect), input.z);
}

template <typename T>
T NORMALIZE(T input, const T range_min, const T range_max) noexcept {
    return MAP<T>(input, range_min, range_max, T(0), T(1));
}

template<typename T>
void CLAMP_IN_RECT(T& inout_x, T& inout_y, T rect_x, T rect_y, T rect_z, T rect_w) noexcept;

template<typename T>
void CLAMP_IN_RECT(T& inout_x, T& inout_y, const Rect<T>& rect) noexcept;

template<typename T>
void CLAMP_IN_RECT(T& inout_x, T& inout_y, const vec4<T>& rect) noexcept;

template<typename T>
bool COORDS_IN_RECT(T input_x, T input_y, T rect_x, T rect_y, T rect_z, T rect_w) noexcept;

template<typename T>
bool COORDS_IN_RECT(T input_x, T input_y, const Rect<T>& rect) noexcept;

template<typename T>
bool COORDS_IN_RECT(T input_x, T input_y, const vec4<T>& rect) noexcept;

constexpr U32 nextPOW2(U32 n) noexcept;
constexpr U32 prevPOW2(U32 n) noexcept;

// Calculate the smalles NxN matrix that can hold the specified
// number of elements. Returns N
constexpr U32 minSquareMatrixSize(U32 elementCount) noexcept;

template <typename T, typename U>
T Lerp(const T v1, const T v2, const U t);

template <typename T>
T Sqrt(T input) noexcept;

template <typename T, typename U>
T Sqrt(U input) noexcept;

///Helper methods to go from a float to packed char and back
constexpr U8 FLOAT_TO_CHAR(const F32 value) noexcept;
constexpr U8 FLOAT_TO_CHAR_SNORM(const F32 value) noexcept;
constexpr F32 CHAR_TO_FLOAT(const U8 value) noexcept;
constexpr F32 CHAR_TO_FLOAT_SNORM(const U8 value) noexcept;

/// Helper method to emulate GLSL
F32 FRACT(const F32 floatValue) noexcept;

// Pack 3 values into 1 float
F32 PACK_FLOAT(const U8 x, const U8 y, const U8 z) noexcept;

// UnPack 3 values from 1 float
void UNPACK_FLOAT(const F32 src, F32& r, F32& g, F32& b) noexcept;

U32 PACK_11_11_10(const F32 x, const F32 y, const F32 z) noexcept;
void UNPACK_11_11_10(const U32 src, F32& x, F32& y, F32& z) noexcept;

// bit manipulation
#define BitSet(arg, posn) (arg |= 1 << posn)
#define BitClr(arg, posn) (arg &= ~(1 << (posn)))
#define BitTst(arg, posn) ((arg & 1 << (posn)) != 0)

#define BitDiff(arg1, arg2) (arg1 ^ arg2)
#define BitCmp(arg1, arg2, posn) ((arg1 << posn) == (arg2 << posn))

// bitmask manipulation
#define BitMaskSet(arg, mask) ((arg) |= (mask))
#define BitMaskClear(arg, mask) ((arg) &= (~(mask)))
#define BitMaskFlip(arg, mask) ((arg) ^= (mask))
#define BitMaskCheck(arg, mask) ((arg) & (mask))

namespace Angle {
/// Return the radian equivalent of the given degree value
template <typename T>
constexpr T DegreesToRadians(const T angleDegrees) noexcept;
/// Return the degree equivalent of the given radian value
template <typename T>
constexpr T RadiansToDegrees(const T angleRadians) noexcept;
/// Returns the specified value. Used only for emphasis
template <typename T>
constexpr T Degrees(const T degrees) noexcept;
/// Returns the specified value. Used only for emphasis
template <typename T>
constexpr T Radians(const T radians) noexcept;
};

namespace Metric {
/// Base value * 1000000000000
template <typename T>
constexpr T Tera(const T a);
/// Base value * 1000000000
template <typename T>
constexpr T Giga(const T a);
/// Base value * 1000000
template <typename T>
constexpr T Mega(const T a);
/// Base value * 1000
template <typename T>
constexpr T Kilo(const T a);
/// Base value * 100
template <typename T>
constexpr T Hecto(const T a);
/// Base value * 10
template <typename T>
constexpr T Deca(const T a);
/// Base value
template <typename T>
constexpr T Base(const T a);
/// Base value * 0.1
template <typename T>
constexpr T Deci(const T a);
/// Base value * 0.01
template <typename T>
constexpr T Centi(const T a);
/// Base value * 0.001
template <typename T>
constexpr T Milli(const T a);
/// Base value * 0.000001
template <typename T>
constexpr T Micro(const T a);
/// Base value * 0.000000001
template <typename T>
constexpr T Nano(const T a);
/// Base value * 0.000000000001
template <typename T>
constexpr T Pico(const T a);

/// Base value * 1000000000000
template <typename T, typename U>
constexpr T Tera(const U a);
/// Base value * 1000000000
template <typename T, typename U>
constexpr T Giga(const U a);
/// Base value * 1000000
template <typename T, typename U>
constexpr T Mega(const U a);
/// Base value * 1000
template <typename T, typename U>
constexpr T Kilo(const U a);
/// Base value * 100
template <typename T, typename U>
constexpr T Hecto(const U a);
/// Base value * 10
template <typename T, typename U>
constexpr T Deca(const U a);
/// Base value
template <typename T, typename U>
constexpr T Base(const U a);
/// Base value * 0.1
template <typename T, typename U>
constexpr T Deci(const U a);
/// Base value * 0.01
template <typename T, typename U>
constexpr T Centi(const U a);
/// Base value * 0.001
template <typename T, typename U>
constexpr T Milli(const U a);
/// Base value * 0.000001
template <typename T, typename U>
constexpr T Micro(const U a);
/// Base value * 0.000000001
template <typename T, typename U>
constexpr T Nano(const U a);
/// Base value * 0.000000000001
template <typename T, typename U>
constexpr T Pico(const U a);
};

namespace Time {
/// Return the passed param without any modification
/// Used only for emphasis
template <typename T>
constexpr T Seconds(const T a);
template <typename T>
constexpr T Milliseconds(const T a);
template <typename T>
constexpr T Microseconds(const T a);
template <typename T>
constexpr T Nanoseconds(const T a);

template <typename T, typename U>
constexpr T Seconds(const U a);
template <typename T, typename U>
constexpr T Milliseconds(const U a);
template <typename T, typename U>
constexpr T Microseconds(const U a);
template <typename T, typename U>
constexpr T Nanoseconds(const U a);

template <typename T = D64, typename U>
constexpr T NanosecondsToSeconds(const U a) noexcept;
template <typename T = D64, typename U>
constexpr T NanosecondsToMilliseconds(const U a) noexcept;
template <typename T = U64, typename U>
constexpr T NanosecondsToMicroseconds(const U a) noexcept;

template <typename T = D64, typename U>
constexpr T MicrosecondsToSeconds(const U a) noexcept;
template <typename T = U64, typename U>
constexpr T MicrosecondsToMilliseconds(const U a) noexcept;
template <typename T = U64, typename U>
constexpr T MicrosecondsToNanoseconds(const U a) noexcept;

template <typename T = D64, typename U>
constexpr T MillisecondsToSeconds(const U a) noexcept;
template <typename T = U64, typename U>
constexpr T MillisecondsToMicroseconds(const U a) noexcept;
template <typename T = U64, typename U>
constexpr T MillisecondsToNanoseconds(const U a) noexcept;

template <typename T = D64, typename U>
constexpr T SecondsToMilliseconds(const U a) noexcept;
template <typename T = U64, typename U>
constexpr T SecondsToMicroseconds(const U a) noexcept;
template <typename T = U64, typename U>
constexpr T SecondsToNanoseconds(const U a) noexcept;

};  // namespace Time

namespace Util {

struct Circle {
    F32 center[2] = {0.0f, 0.0f};
    F32 radius = 1.f;
};

bool IntersectCircles(const Circle& cA, const Circle& cB, vec2<F32>* pointsOut) noexcept;

/// a la Boost
template <typename T>
void Hash_combine(size_t& seed, const T& v) noexcept;

// U = to data type, T = from data type
template <typename U, typename T>
U ConvertData(const T& data);


/** Ogre3D
@brief Normalise the selected rotations to be within the +/-180 degree range.
@details The normalise uses a wrap around,
@details so for example a yaw of 360 degrees becomes 0 degrees, and -190 degrees
becomes 170.
@param normYaw If false, the yaw isn't normalized.
@param normPitch If false, the pitch isn't normalized.
@param normRoll If false, the roll isn't normalized.
*/
void Normalize(vec3<F32>& inputRotation, bool degrees = false,
               bool normYaw = true, bool normPitch = true,
               bool normRoll = true) noexcept;

UColour4  ToByteColour(const FColour4& floatColour) noexcept;
UColour3  ToByteColour(const FColour3& floatColour) noexcept;
vec4<I32> ToIntColour(const FColour4& floatColour) noexcept;
vec3<I32> ToIntColour(const FColour3& floatColour) noexcept;
vec4<U32> ToUIntColour(const FColour4& floatColour) noexcept;
vec3<U32> ToUIntColour(const FColour3& floatColour) noexcept;
FColour4 ToFloatColour(const UColour4& byteColour) noexcept;
FColour3 ToFloatColour(const UColour3& byteColour) noexcept;
FColour4 ToFloatColour(const vec4<U32>& uintColour) noexcept;
FColour3 ToFloatColour(const vec3<U32>& uintColour) noexcept;

void ToByteColour(const FColour4& floatColour, UColour4& colourOut) noexcept;
void ToByteColour(const FColour3& floatColour, UColour3& colourOut) noexcept;
void ToIntColour(const FColour4& floatColour, vec4<I32>& colourOut) noexcept;
void ToIntColour(const FColour3& floatColour, vec3<I32>& colourOut) noexcept;
void ToUIntColour(const FColour4& floatColour, vec4<U32>& colourOut) noexcept;
void ToUIntColour(const FColour3& floatColour, vec3<U32>& colourOut) noexcept;
void ToFloatColour(const UColour4& byteColour, FColour4& colourOut) noexcept;
void ToFloatColour(const UColour3& byteColour, FColour3& colourOut) noexcept;
void ToFloatColour(const vec4<U32>& uintColour, FColour4& colourOut) noexcept;
void ToFloatColour(const vec3<U32>& uintColour, FColour3& colourOut) noexcept;


inline F32 PACK_VEC3_SNORM(const F32 x, const F32 y, const F32 z) noexcept {
    return PACK_FLOAT(FLOAT_TO_CHAR_SNORM(x),
                      FLOAT_TO_CHAR_SNORM(y),
                      FLOAT_TO_CHAR_SNORM(z));
}

inline F32 PACK_VEC3(const F32 x, const F32 y, const F32 z) noexcept {
    return PACK_FLOAT(FLOAT_TO_CHAR(x),
                      FLOAT_TO_CHAR(y),
                      FLOAT_TO_CHAR(z));
}

inline U32 PACK_VEC2(F32 x, F32 y) noexcept {
    const U32 xScaled = to_U32(x * 0xFFFF);
    const U32 yScaled = to_U32(y * 0xFFFF);
    return (xScaled << 16) | (yScaled & 0xFFFF);
}

F32 PACK_VEC3(const vec3<F32>& value) noexcept;

U32 PACK_VEC2(const vec2<F32>& value) noexcept;

U32 PACK_HALF2x16(const vec2<F32>& value);
void UNPACK_HALF2x16(const U32 src, vec2<F32>& value);

U32 PACK_UNORM4x8(const vec4<U8>& value);
void UNPACK_UNORM4x8(const U32 src, vec4<U8>& value);

inline void UNPACK_VEC3(const F32 src, F32& x, F32& y, F32& z) noexcept {
    UNPACK_FLOAT(src, x, y, z);
}

void UNPACK_VEC3(const F32 src, vec3<F32>& res) noexcept;

vec3<F32> UNPACK_VEC3(const F32 src) noexcept;

void UNPACK_VEC2(const U32 src, F32& x, F32& y) noexcept;
void UNPACK_VEC2(const U32 src, vec2<F32>& res) noexcept;

U32 PACK_11_11_10(const vec3<F32>& value) noexcept;

void UNPACK_11_11_10(const U32 src, vec3<F32>& res) noexcept;

};  // namespace Util
};  // namespace Divide

namespace std {
    template<typename T, size_t N>
    struct hash<array<T, N> >
    {
        using argument_type = array<T, N>;
        using result_type = size_t;

        result_type operator()(const argument_type& a) const
        {
            result_type h = 17;
            for (const T& elem : a)
            {
                Divide::Util::Hash_combine(h, elem);
            }
            return h;
        }
    };
};

#endif  //_CORE_MATH_MATH_HELPER_H_

#include "MathHelper.inl"
