/*
   Copyright (c) 2016 DIVIDE-Studio
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

#ifndef _CORE_MATH_MATH_HELPER_INL_
#define _CORE_MATH_MATH_HELPER_INL_

namespace Divide {

template <typename T>
T Random(const T max) {
    return max * rand() * static_cast<T>(INV_RAND_MAX);
}

template <>
inline I32 Random(const I32 max) {
    return rand() % (max + 1);
}

template <typename T>
inline T Random(const T min, const T max) {
    return min + (max - min) * 
           static_cast<T>(INV_RAND_MAX) * 
           static_cast<T>(rand());
}

/// Clamps value n between min and max
template <typename T>
inline void CLAMP(T& n, const T min, const T max) {
    n = std::min(std::max(n, min), max);
}

template <typename T>
inline T CLAMPED(const T& n, const T min, const T max) {
    return std::min(std::max(n, min), max);
}


template<typename T>
inline bool BitCompare(const T bitMask, const T bit) {
    return BitCompare(to_uint(bitMask), to_uint(bit));
}

template<>
inline bool BitCompare<U32>(const U32 bitMask, const U32 bit) {
    return ((bitMask & bit) == bit);
}

inline void SetBit(U32& bitMask, const U32 bit) {
    bitMask |= bit;
}

inline void ClearBit(U32& bitMask, const U32 bit) {
    bitMask &= ~(bit);
}

inline U32 nextPOW2(U32 n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;

    return ++n;
}

template <typename T, typename U>
inline T Lerp(const T v1, const T v2, const U t) {
    return v1 + (v2 - v1 * t);
}

///(thx sqrt[-1] and canuckle of opengl.org forums)

// Helper method to emulate GLSL
inline F32 FRACT(const F32 floatValue) {
    return to_float(fmod(floatValue, 1.0f));
}

///Helper methods to go from a float [0 ... 1] to packed char and back
inline U8 FLOAT_TO_CHAR_SNORM(const F32 value) {
    return to_ubyte(std::min(255, (I32)(value * 256.0f)));
}

inline U8 FLOAT_TO_CHAR(const F32 value) {
    return to_ubyte(((value + 1.0f) * 0.5f) * 255.0f);
}

inline F32 CHAR_TO_FLOAT(const U8 value) {
    return ((value / 255.0f) * 2.0f) - 1.0f;
}

inline F32 CHAR_TO_FLOAT_SNORM(const U8 value) {
    return value / 256.0f;
}

// Pack 3 values into 1 float
inline F32 PACK_FLOAT(const U8 x, const U8 y, const U8 z) {
    static const D64 offset = to_double(1 << 24);

    U32 packedColor = (x << 16) | (y << 8) | z;
    return to_float(to_double(packedColor) / offset);
}

// UnPack 3 values from 1 float
inline void UNPACK_FLOAT(const F32 src, F32& r, F32& g, F32& b) {
    r = FRACT(src);
    g = FRACT(src * 256.0f);
    b = FRACT(src * 65536.0f);

    // Unpack to the -1..1 range
    r = (r * 2.0f) - 1.0f;
    g = (g * 2.0f) - 1.0f;
    b = (b * 2.0f) - 1.0f;
}

namespace Angle {
template <typename T>
constexpr T DegreesToRadians(const T angleDegrees) {
    return static_cast<T>(angleDegrees * M_PIDIV180);
}

template <typename T>
constexpr T RadiansToDegrees(const T angleRadians) {
    return static_cast<T>(angleRadians * M_180DIVPI);
}

/// Returns the specified value. Used only for emphasis
template <typename T>
constexpr T Degrees(const T degrees) {
    return degrees;
}

/// Returns the specified value. Used only for emphasis
template <typename T>
constexpr T Radians(const T radians) {
    return radians;
}

};  // namespace Angle

namespace Metric {
template <typename T>
constexpr T Tera(const T a) {
    return static_cast<T>(a * 1'000'000'000'000.0);
}

template <typename T>
constexpr T Giga(const T a) {
    return static_cast<T>(a * 1'000'000'000.0);
}

template <typename T>
constexpr T Mega(const T a) {
    return static_cast<T>(a * 1'000'000.0);
}

template <typename T>
constexpr T Kilo(const T a) {
    return static_cast<T>(a * 1'000.0);
}

template <typename T>
constexpr T Hecto(const T a) {
    return static_cast<T>(a * 100.0);
}

template <typename T>
constexpr T Deca(const T a) {
    return static_cast<T>(a * 10.0);
}

template <typename T>
constexpr T Base(const T a) {
    return a;
}

template <typename T>
constexpr T Deci(const T a) {
    return static_cast<T>(a * 0.1);
}

template <typename T>
constexpr T Centi(const T a) {
    return static_cast<T>(a * 0.01);
}

template <typename T>
constexpr T Milli(const T a) {
    return static_cast<T>(a * 0.001);
}

template <typename T>
constexpr T Micro(const T a) {
    return static_cast<T>(a * 0.000'001);
}

template <typename T>
constexpr T Nano(const T a) {
    return static_cast<T>(a * 0.000'000'001);
}

template <typename T>
constexpr T Pico(const T a) {
    return static_cast<T>(a * 0.000'000'000'001);
}
};  // namespace Metric

namespace Time {
template <typename T>
T Seconds(const T a) {
    return a;
}

template <typename T>
T Milliseconds(const T a) {
    return a;
}

template <typename T>
U64 Microseconds(const T a) {
    return static_cast<U64>(a);
}

template <typename T>
/*constexpr*/ T MicrosecondsToSeconds(const U64 a) {
    return Metric::Micro(static_cast<T>(a));
}

template <typename T>
/*constexpr*/ T MicrosecondsToMilliseconds(const U64 a) {
    return Metric::Milli(static_cast<T>(a));
}

template <typename T>
/*constexpr*/ T SecondsToMilliseconds(const T a) {
    return Metric::Kilo(a);
}

template <typename T>
/*constexpr*/ U64 SecondsToMicroseconds(const T a) {
    return static_cast<U64>(Metric::Mega(a));
}

template <typename T>
/*constexpr*/ U64 MillisecondsToMicroseconds(const T a) {
    return static_cast<U64>(Metric::Kilo(a));
}

template <typename T>
/*constexpr*/ T MillisecondsToSeconds(const T a) {
    return Metric::Milli(a);
}
};  // namespace Time

namespace Util {
/// a la Boost
template <typename T>
void Hash_combine(U32& seed, const T& v) {
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

inline void ReplaceStringInPlace(stringImpl& subject, 
                                 const stringImpl& search,
                                 const stringImpl& replace) {
    stringAlg::stringSize pos = 0;
    while ((pos = subject.find(search, pos)) != stringImpl::npos) {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
    }
}

// U = to data type, T = from data type
template <typename U, typename T>
U ConvertData(const T& data) {
    U targetValue;
    std::istringstream iss(data);
    iss >> targetValue;
    DIVIDE_ASSERT(!iss.fail(), "Util::convertData error : invalid conversion!");
    return targetValue;
}

/// http://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring
inline stringImpl Ltrim(const stringImpl& s) {
    stringImpl temp(s);
    return Ltrim(temp);
}

inline stringImpl& Ltrim(stringImpl& s) {
    s.erase(std::begin(s),
            std::find_if(std::begin(s), std::end(s),
                         std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
}

inline stringImpl Rtrim(const stringImpl& s) {
    stringImpl temp(s);
    return Rtrim(temp);
}

inline stringImpl& Rtrim(stringImpl& s) {
    s.erase(
        std::find_if(std::rbegin(s), std::rend(s),
                     std::not1(std::ptr_fun<int, int>(std::isspace))).base(),
        std::end(s));
    return s;
}

inline stringImpl& Trim(stringImpl& s) {
    return Ltrim(Rtrim(s));
}

inline stringImpl Trim(const stringImpl& s) {
    stringImpl temp(s);
    return Trim(temp);
}

template<class FwdIt, class Compare>
void insertion_sort(FwdIt first, FwdIt last, Compare cmp)
{
    for (auto it = first; it != last; ++it) {
        auto const insertion = std::upper_bound(first, it, *it, cmp);
        std::rotate(insertion, it, std::next(it));
    }
}

};  // namespace Util
};  // namespace Divide

#endif  //_CORE_MATH_MATH_HELPER_INL_