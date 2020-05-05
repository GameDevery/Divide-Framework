/* Copyright (c) 2018 DIVIDE-Studio
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

#ifndef _CORE_MATH_MATH_VECTORS_INL_
#define _CORE_MATH_MATH_VECTORS_INL_

namespace Divide {

namespace {
    //ref: http://stackoverflow.com/questions/6042399/how-to-compare-m128-types
    FORCE_INLINE bool fneq128(__m128 const& a, __m128 const& b) noexcept
    {
        // returns true if at least one element in a is not equal to 
        // the corresponding element in b
        return _mm_movemask_ps(_mm_cmpeq_ps(a, b)) != 0xF;
    }

    FORCE_INLINE bool fneq128(__m128 const& a, __m128 const& b, F32 epsilon) noexcept
    {
        // epsilon vector
        const auto eps = _mm_set1_ps(epsilon);
        // absolute of difference of a and b
        const auto abd = _mm_andnot_ps(_mm_set1_ps(-0.0f), _mm_sub_ps(a, b));
        // compare abd to eps
        // returns true if one of the elements in abd is not less than 
        // epsilon
        return _mm_movemask_ps(_mm_cmplt_ps(abd, eps)) != 0xF;
    }

    //ref: https://www.opengl.org/discussion_boards/showthread.php/159586-my-SSE-code-is-slower-than-normal-why
    FORCE_INLINE __m128 DOT_SIMD(const __m128 &a, const __m128 &b) noexcept
    {
        __m128 r;

        r = _mm_mul_ps(a, b);
        r = _mm_add_ps(_mm_movehl_ps(r, r), r);
        r = _mm_add_ss(_mm_shuffle_ps(r, r, 1), r);

        return r;
    }

    FORCE_INLINE __m128 SIMPLE_DOT(__m128 a, __m128 b) noexcept
    {
        a = _mm_mul_ps(a, b);
        b = _mm_hadd_ps(a, a);
        return _mm_hadd_ps(b, b);
    }
}
/*
*  useful vector functions
*/
/// general vec2 cross function
template <typename T>
inline vec2<T> Cross(const vec2<T> &v1, const vec2<T> &v2) noexcept {
    return v1.x * v2.y - v1.y * v2.x;
}

template <typename T>
inline vec2<T> Inverse(const vec2<T> &v) noexcept {
    return vec2<T>(v.y, v.x);
}

template <typename T>
inline vec2<T> Normalize(vec2<T> &vector) {
    return vector.normalize();
}

template <typename T>
//[[nodiscard]]
inline vec2<T> Normalized(const vec2<T> &vector) {
    return vec2<T>(vector).normalize();
}

/// multiply a vector by a value
template <typename T>
inline vec2<T> operator*(T fl, const vec2<T> &v) noexcept {
    return vec2<T>(v.x * fl, v.y * fl);
}

/// general vec2 dot product
template <typename T>
inline T Dot(const vec2<T> &a, const vec2<T> &b) noexcept {
    return (a.x * b.x + a.y * b.y);
}

template <typename T>
inline void OrthoNormalize(vec2<T> &n, vec2<T> &u) {
    n.normalize();
    u.set(Cross(Normalized(Cross(n, u)), n));
}

template <typename T>
inline vec3<T> Normalize(vec3<T> &vector) {
    return vector.normalize();
}

template <typename T>
//[[nodiscard]]
inline vec3<T> Normalized(const vec3<T> &vector) {
    return vec3<T>(vector).normalize();
}

/// multiply a vector by a value
template <typename T>
inline vec3<T> operator*(T fl, const vec3<T> &v) noexcept {
    return vec3<T>(v.x * fl, v.y * fl, v.z * fl);
}

/// general vec3 dot product
template <typename T>
inline T Dot(const vec3<T> &a, const vec3<T> &b) noexcept {
    return (a.x * b.x + a.y * b.y + a.z * b.z);
}

/// general vec3 cross function
template <typename T>
inline vec3<T> Cross(const vec3<T> &v1, const vec3<T> &v2) noexcept {
    return vec3<T>(v1.y * v2.z - v1.z * v2.y, 
                   v1.z * v2.x - v1.x * v2.z,
                   v1.x * v2.y - v1.y * v2.x);
}

template <typename T>
inline vec3<T> Inverse(const vec3<T> &v) noexcept {
    return vec3<T>(v.z, v.y, v.x);
}

template <typename T>
inline vec3<T> Perpendicular(const vec3<T>& v) noexcept {
    T min = std::abs(v.x);
    vec3<T> cardinalAxis = WORLD_X_AXIS;

    if (std::abs(v.y) < min) {
        min = std::abs(v.y);
        cardinalAxis = WORLD_Y_AXIS;
    }

    if (std::abs(v.z) < min) {
        cardinalAxis = WORLD_Z_AXIS;
    }

    return Cross(v, cardinalAxis);
}

template <typename T>
inline void OrthoNormalize(vec3<T> &n, vec3<T> &u) {
    n.normalize();
    u.set(Cross(Normalized((Cross(n, u))), n));
}

/// min/max functions
template <typename T>
inline vec4<T> Min(const vec4<T> &v1, const vec4<T> &v2) noexcept {
    return vec4<T>(std::min(v1.x, v2.x), std::min(v1.y, v2.y),
                   std::min(v1.z, v2.z), std::min(v1.w, v2.w));
}

template <typename T>
inline vec4<T> Max(const vec4<T> &v1, const vec4<T> &v2) noexcept {
    return vec4<T>(std::max(v1.x, v2.x), std::max(v1.y, v2.y),
                   std::max(v1.z, v2.z), std::max(v1.w, v2.w));
}

template <typename T>
inline vec4<T> Normalize(vec4<T> &vector) {
    return vector.normalize();
}

template <typename T>
//[[nodiscard]]
inline vec4<T> Normalized(const vec4<T> &vector) {
    return vec4<T>(vector).normalize();
}

/// multiply a vector by a value
template <typename T>
inline vec4<T> operator*(T fl, const vec4<T> &v) noexcept {
    return v * fl;
}

/*
*  vec2 inline definitions
*/

/// return the squared distance of the vector
template <typename T>
inline T vec2<T>::lengthSquared() const noexcept {
    return Divide::Dot(*this, *this);
}

/// compute the vector's distance to another specified vector
template <typename T>
inline T vec2<T>::distance(const vec2 &v) const {
    return Divide::Sqrt(distanceSquared(v));
}

/// compute the vector's squared distance to another specified vector
template <typename T>
inline T vec2<T>::distanceSquared(const vec2 &v) const noexcept {
    const vec2 d = v - *this;
    return Divide::Dot(d, d);
}

/// convert the vector to unit length
template <typename T>
inline vec2<T>& vec2<T>::normalize() {
    const T l = this->length();

    if (l >= EPSILON_F32) {
        *this *= 1.0f / l;
    }

    return *this;
}

/// get the smallest value of X or Y
template <typename T>
inline T vec2<T>::minComponent() const noexcept {
    return std::min(x, y);
}
/// get the largest value of X or Y
template <typename T>
inline T vec2<T>::maxComponent() const noexcept {
    return std::max(x, y);
}

/// compare 2 vectors
template <typename T>
template <typename U>
inline bool vec2<T>::compare(const vec2<U> &v) const noexcept {
    return COMPARE(this->x, v.x) &&
           COMPARE(this->y, v.y);
}

/// compare 2 vectors using the given tolerance
template <typename T>
template <typename U>
inline bool vec2<T>::compare(const vec2<U> &v, U epsi) const noexcept {
    return (COMPARE_TOLERANCE(this->x, v.x, epsi) &&
            COMPARE_TOLERANCE(this->y, v.y, epsi));
}

/// return the projection factor from *this to the line determined by points vA
/// and vB
template <typename T>
inline T vec2<T>::projectionOnLine(const vec2 &vA, const vec2 &vB) const {
    vec2 v(vB - vA);
    return v.dot(*this - vA) / v.dot(v);
}

/// get the dot product between this vector and the specified one
template <typename T>
inline T vec2<T>::dot(const vec2 &v) const noexcept {
    return ((this->x * v.x) + (this->y * v.y));
}

/// round both values
template <typename T>
inline void vec2<T>::round() {
    set((T)std::roundf(this->x), (T)std::roundf(this->y));
}

/// export the vector's components in the first 2 positions of the specified
/// array
template <typename T>
inline void vec2<T>::get(T *v) const {
    v[0] = (T) this->_v[0];
    v[1] = (T) this->_v[1];
}

/// return the coordinates of the closest point from *this to the line
/// determined by points vA and vB
template <typename T>
inline vec2<T> vec2<T>::closestPointOnLine(const vec2 &vA, const vec2 &vB) {
    return (((vB - vA) * this->projectionOnLine(vA, vB)) + vA);
}

/// return the coordinates of the closest point from *this to the segment
/// determined by points vA and vB
template <typename T>
inline vec2<T> vec2<T>::closestPointOnSegment(const vec2 &vA, const vec2 &vB) {
    const T factor = this->projectionOnLine(vA, vB);

    if (factor <= 0) return vA;

    if (factor >= 1) return vB;

    return (((vB - vA) * factor) + vA);
}

/// lerp between this and the specified vector by the specified amount
template <typename T>
inline void vec2<T>::lerp(const vec2 &v, T factor) noexcept {
    set(Lerp(*this, v, factor));
}

/// lerp between this and the specified vector by the specified amount for each
/// component
template <typename T>
inline void vec2<T>::lerp(const vec2 &v, const vec2 &factor) noexcept {
    set(Lerp(*this, v, factor));
}

/// linear interpolation between 2 vectors
template <typename T, typename U>
inline vec2<T> Lerp(const vec2<T> &u, const vec2<T> &v, U factor) noexcept {
    return { Lerp(u.x, v.x, factor), Lerp(u.y, v.y, factor) };
}

/// linear interpolation between 2 vectors based on separate x and y factors
template <typename T>
inline vec2<T> Lerp(const vec2<T> &u, const vec2<T> &v, const vec2<T> &factor) noexcept {
    return { Lerp(u.x, v.x, factor.x), Lerp(u.y, v.y, factor.y) };
}

/*
*  vec3 inline definitions
*/

/// compare 2 vectors
template <typename T>
template <typename U>
inline bool vec3<T>::compare(const vec3<U> &v) const noexcept {
    return COMPARE(this->x, v.x) &&
           COMPARE(this->y, v.y) &&
           COMPARE(this->z, v.z);
}

/// compare 2 vectors within the specified tolerance
template <typename T>
template <typename U>
inline bool vec3<T>::compare(const vec3<U> &v, U epsi) const noexcept {
    return COMPARE_TOLERANCE(this->x, v.x, epsi) &&
           COMPARE_TOLERANCE(this->y, v.y, epsi) &&
           COMPARE_TOLERANCE(this->z, v.z, epsi);
}

/// uniform vector: x = y = z
template <typename T>
inline bool vec3<T>::isUniform() const noexcept {
    return COMPARE(this->x, this->y) && COMPARE(this->y, this->z);
}

/// return the squared distance of the vector
template <typename T>
inline T vec3<T>::lengthSquared() const noexcept {
    return Divide::Dot(*this, *this);
}

/// transform the vector to unit length
template <typename T>
inline vec3<T>& vec3<T>::normalize() {
    const T l = this->length();

    if (l >= EPSILON_F32) {
        // multiply by the inverse length
        *this *= (1.0f / l);
    }

    return *this;
}

/// get the smallest value of X, Y or Z
template <typename T>
inline T vec3<T>::minComponent() const noexcept {
    return std::min(x, std::min(y, z));
}

/// get the largest value of X, Y or Z
template <typename T>
inline T vec3<T>::maxComponent() const noexcept {
    return std::max(x, std::max(y, z));
}

/// set this vector to be equal to the cross of the 2 specified vectors
template <typename T>
inline void vec3<T>::cross(const vec3 &v1, const vec3 &v2) noexcept {
    this->x = v1.y * v2.z - v1.z * v2.y;
    this->y = v1.z * v2.x - v1.x * v2.z;
    this->z = v1.x * v2.y - v1.y * v2.x;
}

/// set this vector to be equal to the cross between itself and the specified
/// vector
template <typename T>
inline void vec3<T>::cross(const vec3 &v2) noexcept {
    this->cross(*this, v2);
}

/// calculate the dot product between this vector and the specified one
template <typename T>
inline T vec3<T>::dot(const vec3 &v) const noexcept {
    return Divide::Dot(*this, v);
}

/// compute the vector's distance to another specified vector
template <typename T>
inline T vec3<T>::distance(const vec3 &v) const noexcept {
    return Divide::Sqrt(distanceSquared(v));
}

/// compute the vector's squared distance to another specified vector
template <typename T>
inline T vec3<T>::distanceSquared(const vec3 &v) const noexcept {
    const vec3 d = v - *this;
    return Divide::Dot(d, d);
}

/// returns the angle in radians between '*this' and 'v'
template <typename T>
inline T vec3<T>::angle(vec3 &v) const {
    const T angle = (T)std::abs(std::acos(this->dot(v) / (this->length() * v.length())));
    return std::max(angle, EPSILON_F32);
}

/// get the direction vector to the specified point
template <typename T>
inline vec3<T> vec3<T>::direction(const vec3 &u) const {
    return Normalized(vec3(u.x - this->x, u.y - this->y, u.z - this->z));
}

/// project this vector on the line defined by the 2 points(A, B)
template <typename T>
inline T vec3<T>::projectionOnLine(const vec3 &vA, const vec3 &vB) const {
    const vec3 vector(vB - vA);
    return vector.dot(*this - vA) / vector.dot(vector);
}

/// lerp between this and the specified vector by the specified amount
template <typename T>
inline void vec3<T>::lerp(const vec3 &v, T factor) noexcept {
    set(Lerp(*this, v, factor));
}

/// lerp between this and the specified vector by the specified amount for each
/// component
template <typename T>
inline void vec3<T>::lerp(const vec3 &v, const vec3 &factor) noexcept {
    set(Lerp(*this, v, factor));
}

/// rotate this vector on the X axis
template <typename T>
inline void vec3<T>::rotateX(D64 radians) {
    this->y = static_cast<T>(std::cos(radians) * this->y +
                             std::sin(radians) * this->z);
    this->z = static_cast<T>(-std::sin(radians) * this->y +
                             std::cos(radians) * this->z);
}

/// rotate this vector on the Y axis
template <typename T>
inline void vec3<T>::rotateY(D64 radians) {
    this->x = static_cast<T>(std::cos(radians) * this->x -
                             std::sin(radians) * this->z);
    this->z = static_cast<T>(std::sin(radians) * this->x +
                             std::cos(radians) * this->z);
}

/// rotate this vector on the Z axis
template <typename T>
inline void vec3<T>::rotateZ(D64 radians) {
    this->x = static_cast<T>(std::cos(radians) * this->x +
                             std::sin(radians) * this->y);
    this->y = static_cast<T>(-std::sin(radians) * this->x +
                             std::cos(radians) * this->y);
}

/// round all three values
template <typename T>
inline void vec3<T>::round() {
    set(static_cast<T>(std::roundf(this->x)),
        static_cast<T>(std::roundf(this->y)),
        static_cast<T>(std::roundf(this->z)));
}

/// swap the components  of this vector with that of the specified one
template <typename T>
inline void vec3<T>::swap(vec3 &iv) noexcept {
    std::swap(this->x, iv.x);
    std::swap(this->y, iv.y);
    std::swap(this->z, iv.z);
}

/// swap the components  of this vector with that of the specified one
template <typename T>
inline void vec3<T>::swap(vec3 *iv) noexcept {
    std::swap(this->x, iv->x);
    std::swap(this->y, iv->y);
    std::swap(this->z, iv->z);
}

/// export the vector's components in the first 3 positions of the specified
/// array
template <typename T>
inline void vec3<T>::get(T *v) const noexcept {
    v[0] = static_cast<T>(this->_v[0]);
    v[1] = static_cast<T>(this->_v[1]);
    v[2] = static_cast<T>(this->_v[2]);
}

/// this calculates a vector between the two specified points and returns the
/// result
template <typename T>
inline vec3<T> vec3<T>::vector(const vec3 &vp1, const vec3 &vp2) const noexcept {
    return vec3(vp1.x - vp2.x, vp1.y - vp2.y, vp1.z - vp2.z);
}

/// return the closest point on the line defined by the 2 points (A, B) and this
/// vector
template <typename T>
inline vec3<T> vec3<T>::closestPointOnLine(const vec3 &vA, const vec3 &vB) {
    return (((vB - vA) * this->projectionOnLine(vA, vB)) + vA);
}

/// return the closest point on the line segment created between the 2 points
/// (A, B) and this vector
template <typename T>
inline vec3<T> vec3<T>::closestPointOnSegment(const vec3 &vA, const vec3 &vB) {
    const T factor = this->projectionOnLine(vA, vB);

    if (factor <= 0.0f) return vA;

    if (factor >= 1.0f) return vB;

    return (((vB - vA) * factor) + vA);
}

/// lerp between the 2 specified vectors by the specified amount
template <typename T, typename U>
inline vec3<T> Lerp(const vec3<T> &u, const vec3<T> &v, U factor) noexcept {
    return { Lerp(u.x, v.x, factor), Lerp(u.y, v.y, factor), Lerp(u.z, v.z, factor) };
}

/// lerp between the 2 specified vectors by the specified amount for each
/// component
template <typename T>
inline vec3<T> Lerp(const vec3<T> &u, const vec3<T> &v, const vec3<T> &factor) noexcept {
    return { Lerp(u.x, v.x, factor.x), Lerp(u.y, v.y, factor.y), Lerp(u.z, v.z, factor.z) };
}
/*
*  vec4 inline definitions
*/

template<>
template<>
inline const vec4<F32> vec4<F32>::operator-(F32 _f) const noexcept {
    return vec4<F32>(_mm_sub_ps(_reg._reg, _mm_set1_ps(_f)));
}

template<>
template<>
inline vec4<F32>& vec4<F32>::operator-=(F32 _f) noexcept {
    _reg._reg = _mm_sub_ps(_reg._reg, _mm_set1_ps(_f));
    return *this;
}

template<>
template<>
inline const vec4<F32> vec4<F32>::operator+(F32 _f) const noexcept {
    return vec4<F32>(_mm_add_ps(_reg._reg, _mm_set1_ps(_f)));
}

template<>
template<>
inline vec4<F32>& vec4<F32>::operator+=(F32 _f) noexcept  {
    _reg._reg = _mm_add_ps(_reg._reg, _mm_set1_ps(_f));
    return *this;
}

template<>
template<>
inline const vec4<F32> vec4<F32>::operator*(F32 _f) const noexcept {
    return vec4<F32>(_mm_mul_ps(_reg._reg, _mm_set1_ps(_f)));
}

template<>
template<>
inline vec4<F32>& vec4<F32>::operator*=(F32 _f) noexcept {
    _reg._reg = _mm_mul_ps(_reg._reg, _mm_set1_ps(_f));
    return *this;
}

template<>
template<>
inline const vec4<F32> vec4<F32>::operator/(F32 _f) const noexcept {
    return vec4<F32>(_mm_div_ps(_reg._reg, _mm_set1_ps(_f)));
}

template<>
template<>
inline vec4<F32>& vec4<F32>::operator/=(F32 _f) noexcept {
    _reg._reg = _mm_div_ps(_reg._reg, _mm_set1_ps(_f));
    return *this;
}

template<>
template<>
inline const vec4<F32> vec4<F32>::operator+(const vec4<F32> &v) const noexcept {
    return vec4<F32>(_mm_add_ps(_reg._reg, v._reg._reg));
}

template<>
template<>
inline vec4<F32>& vec4<F32>::operator+=(const vec4<F32>& v) noexcept {
    _reg._reg = _mm_add_ps(_reg._reg, v._reg._reg);
    return *this;
}

template<>
template<>
inline const vec4<F32> vec4<F32>::operator-(const vec4<F32> &v) const noexcept {
    return vec4<F32>(_mm_sub_ps(_reg._reg, v._reg._reg));
}

template<>
template<>
inline vec4<F32>& vec4<F32>::operator-=(const vec4<F32>& v) noexcept {
    _reg._reg = _mm_sub_ps(_reg._reg, v._reg._reg);
    return *this;
}

template<>
template<>
inline const vec4<F32> vec4<F32>::operator/(const vec4<F32> &v) const noexcept {
    return vec4<F32>(_mm_div_ps(_reg._reg, v._reg._reg));
}

template<>
template<>
inline vec4<F32>& vec4<F32>::operator/=(const vec4<F32>& v) noexcept {
    _reg._reg = _mm_div_ps(_reg._reg, v._reg._reg);
    return *this;
}

template<>
template<>
inline const vec4<F32> vec4<F32>::operator*(const vec4<F32>& v) const noexcept {
    return vec4<F32>(_mm_mul_ps(_reg._reg, v._reg._reg));
}

template<>
template<>
inline vec4<F32>& vec4<F32>::operator*=(const vec4<F32>& v) noexcept {
    _reg._reg = _mm_mul_ps(_reg._reg, v._reg._reg);
    return *this;
}

/// compare 2 vectors
template <typename T>
template <typename U>
inline bool vec4<T>::compare(const vec4<U> &v) const noexcept {
    return COMPARE(this->x, v.x) &&
           COMPARE(this->y, v.y) &&
           COMPARE(this->z, v.z) &&
           COMPARE(this->w, v.w);
}

template <>
template <>
inline bool vec4<F32>::compare(const vec4<F32> &v) const noexcept {
    // returns true if at least one element in a is not equal to 
    // the corresponding element in b
    return compare(v, EPSILON_F32);
}

/// compare this vector with the one specified and see if they match within the specified amount
template <typename T>
template <typename U>
inline bool vec4<T>::compare(const vec4<U> &v, U epsi) const noexcept{
    if_constexpr(std::is_same<T, U>::value && std::is_same<U, F32>::value) {
        return !fneq128(_reg._reg, v._reg._reg, epsi);
    } else {
        return (COMPARE_TOLERANCE(this->x, v.x, epsi) &&
                COMPARE_TOLERANCE(this->y, v.y, epsi) &&
                COMPARE_TOLERANCE(this->z, v.z, epsi) &&
                COMPARE_TOLERANCE(this->w, v.w, epsi));
    }
}

/// round all four values
template <typename T>
inline void vec4<T>::round() {
    set((T)std::roundf(this->x), (T)std::roundf(this->y),
        (T)std::roundf(this->z), (T)std::roundf(this->w));
}

/// swap this vector's values with that of the specified vector
template <typename T>
inline void vec4<T>::swap(vec4 *iv) noexcept {
    std::swap(this->x, iv->x);
    std::swap(this->y, iv->y);
    std::swap(this->z, iv->z);
    std::swap(this->w, iv->w);
}

template <>
inline void vec4<F32>::swap(vec4<F32> *iv) noexcept {
    std::swap(_reg._reg, iv->_reg._reg);
}

/// swap this vector's values with that of the specified vector
template <typename T>
inline void vec4<T>::swap(vec4 &iv) noexcept {
    swap(&iv);
}

/// general vec4 dot product
template <typename T>
inline T Dot(const vec4<T> &a, const vec4<T> &b) noexcept {
    return (a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w);
}

template <>
inline F32 Dot(const vec4<F32> &a, const vec4<F32> &b) noexcept {
    return _mm_cvtss_f32(DOT_SIMD(a._reg._reg, b._reg._reg));
}

/// calculate the dot product between this vector and the specified one
template <typename T>
inline T vec4<T>::dot(const vec4 &v) const noexcept {
    return Divide::Dot(*this, v);
}

template<>
inline F32 vec4<F32>::length() const noexcept {
    return Divide::Sqrt<F32>(DOT_SIMD(_reg._reg, _reg._reg));
}

/// return the squared distance of the vector
template <typename T>
inline T vec4<T>::lengthSquared() const noexcept {
    return Dot(*this, *this);
}
/// transform the vector to unit length
template <typename T>
inline vec4<T>& vec4<T>::normalize() {
    const T l = this->length();

    if (l >= EPSILON_F32) {
        // multiply by the inverse length
        *this *= (1.0f / l);
    }

    return *this;
}

template <>
inline vec4<F32>& vec4<F32>::normalize() {
    _reg._reg = _mm_mul_ps(_reg._reg, _mm_rsqrt_ps(SIMPLE_DOT(_reg._reg, _reg._reg)));
    return *this;
}

/// get the smallest value of X, Y, Z or W
template <typename T>
inline T vec4<T>::minComponent() const noexcept {
    return std::min(x, std::min(y, std::min(z, w)));
}
/// get the largest value of X, Y, Z or W
template <typename T>
inline T vec4<T>::maxComponent() const noexcept {
    return std::max(x, std::max(y, std::min(z, w)));
}

/// lerp between this and the specified vector by the specified amount
template <typename T>
inline void vec4<T>::lerp(const vec4 &v, T factor) noexcept {
    set(Lerp(*this, v, factor));
}

/// lerp between this and the specified vector by the specified amount for each
/// component
template <typename T>
inline void vec4<T>::lerp(const vec4 &v, const vec4 &factor) noexcept {
    set(Lerp(*this, v, factor));
}
/// lerp between the 2 vectors by the specified amount
template <typename T>
inline vec4<T> Lerp(const vec4<T> &u, const vec4<T> &v, T factor) noexcept {
    return { Lerp(u.x, v.x, factor), Lerp(u.y, v.y, factor), Lerp(u.z, v.z, factor), Lerp(u.w, v.w, factor) };
}

/// lerp between the 2 specified vectors by the specified amount for each
/// component
template <typename T>
inline vec4<T> Lerp(const vec4<T> &u, const vec4<T> &v, const vec4<T> &factor) noexcept {
    return { Lerp(u.x, v.x, factor.x), Lerp(u.y, v.y, factor.y), Lerp(u.z, v.z, factor.z), Lerp(u.w, v.w, factor.w) };
}

template <typename Type>
inline vec2<Type> Random(const vec2<Type>& min, const vec2<Type>& max) {
    return vec2<Type>(Random(min.x, max.x), Random(min.y, max.y));
}

template <typename Type>
inline vec3<Type> Random(const vec3<Type>& min, const vec3<Type>& max) {
    return vec3<Type>(Random(min.x, max.x), Random(min.y, max.y),
                      Random(min.z, max.z));
}

template <typename Type>
inline vec4<Type> Random(const vec4<Type>& min, const vec4<Type>& max) {
    return vec4<Type>(Random(min.x, max.x), Random(min.y, max.y),
                      Random(min.z, max.z), Random(min.w, max.w));
}
/*
template<>
const vec4<F32> vec4<F32>::operator-(F32 _f) const {
    return vec4<F32>(simd_vector<F32>(_mm_sub_ps(this->_reg._reg, _mm_set1_ps(_f))));
}

template<>
const vec4<F32> vec4<F32>::operator+(F32 _f) const {
    return vec4<F32>(simd_vector<F32>(_mm_add_ps(this->_reg._reg, _mm_set1_ps(_f))));
}

template<>
const vec4<F32> vec4<F32>::operator*(F32 _f) const {
    return vec4<F32>(simd_vector<F32>(_mm_mul_ps(this->_reg._reg, _mm_set1_ps(_f))));
}

template<>
const vec4<F32> vec4<F32>::operator/(F32 _f) const {
    if (IS_ZERO(_f)) {
        return *this;
    }

    return vec4<F32>(simd_vector<F32>(_mm_div_ps(this->_reg._reg, _mm_set1_ps(_f))));
}

template<>
const vec4<F32> vec4<F32>::operator+(const vec4<F32> &v) const {
    return vec4<F32>(simd_vector<F32>(_mm_add_ps(this->_reg._reg, v._reg._reg)));
}

template<>
const vec4<F32> vec4<F32>::operator-(const vec4<F32> &v) const {
    return vec4<F32>(simd_vector<F32>(_mm_sub_ps(this->_reg._reg, v._reg._reg)));
}

template<>
const vec4<F32> vec4<F32>::operator/(const vec4<F32> &v) const {
    return vec4<F32>(simd_vector<F32>(_mm_div_ps(this->_reg._reg, v._reg._reg)));
}

template<>
const vec4<F32> vec4<F32>::operator*(const vec4<F32> &v) const {
    return vec4<F32>(simd_vector<F32>(_mm_mul_ps(this->_reg._reg, v._reg._reg)));
}

template<>
vec4<F32>& vec4<F32>::operator*=(const vec4<F32> &v) {
    this->_reg._reg = _mm_mul_ps(this->_reg._reg, v._reg._reg);
    return *this;
}

template<>
vec4<F32>& vec4<F32>::operator/=(const vec4<F32> &v) {
    this->_reg._reg = _mm_div_ps(this->_reg._reg, v._reg._reg);
    return *this;
}

template<>
vec4<F32>& vec4<F32>::operator+=(const vec4<F32> &v) {
    this->_reg._reg = _mm_add_ps(this->_reg._reg, v._reg._reg);
    return *this;
}

template<>
vec4<F32>& vec4<F32>::operator-=(const vec4<F32> &v) {
    this->_reg._reg = _mm_sub_ps(this->_reg._reg, v._reg._reg);
    return *this;
}

template<>
inline void vec4<F32>::setV(const F32 *v) {
    this->_reg._reg = _mm_load_ps(v);
}
*/
};  // namespace Divide

#endif //_CORE_MATH_MATH_VECTORS_INL_
