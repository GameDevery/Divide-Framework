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

#ifndef _QUATERNION_INL_
#define _QUATERNION_INL_

namespace Divide {

namespace {
    //ref: http://stackoverflow.com/questions/18542894/how-to-multiply-two-quaternions-with-minimal-instructions?lq=1
    inline static __m128 multiplynew(__m128 xyzw, __m128 abcd) noexcept {
        /* The product of two quaternions is:                                 */
        /* (X,Y,Z,W) = (xd+yc-zb+wa, -xc+yd+za+wb, xb-ya+zd+wc, -xa-yb-zc+wd) */
        const __m128 wzyx = _mm_shuffle_ps(xyzw, xyzw, _MM_SHUFFLE(0, 1, 2, 3));
        const __m128 baba = _mm_shuffle_ps(abcd, abcd, _MM_SHUFFLE(0, 1, 0, 1));
        const __m128 dcdc = _mm_shuffle_ps(abcd, abcd, _MM_SHUFFLE(2, 3, 2, 3));

        /* variable names below are for parts of componens of result (X,Y,Z,W) */

        /* nX stands for -X and similarly for the other components             */
        /* znxwy  = (xb - ya, zb - wa, wd - zc, yd - xc) */
        const __m128 ZnXWY = _mm_hsub_ps(_mm_mul_ps(xyzw, baba), _mm_mul_ps(wzyx, dcdc));

        /* xzynw  = (xd + yc, zd + wc, wb + za, yb + xa) */
        const __m128 XZYnW = _mm_hadd_ps(_mm_mul_ps(xyzw, dcdc), _mm_mul_ps(wzyx, baba));

        /* _mm_shuffle_ps(XZYnW, ZnXWY, _MM_SHUFFLE(3,2,1,0)) */
        /*      = (xd + yc, zd + wc, wd - zc, yd - xc)        */
        /* _mm_shuffle_ps(ZnXWY, XZYnW, _MM_SHUFFLE(2,3,0,1)) */
        /*      = (zb - wa, xb - ya, yb + xa, wb + za)        */

        /* _mm_addsub_ps adds elements 1 and 3 and subtracts elements 0 and 2, so we get: */
        /* _mm_addsub_ps(*, *) = (xd+yc-zb+wa, xb-ya+zd+wc, wd-zc+yb+xa, yd-xc+wb+za)     */

        const __m128 XZWY = _mm_addsub_ps(_mm_shuffle_ps(XZYnW, ZnXWY, _MM_SHUFFLE(3, 2, 1, 0)),
                                          _mm_shuffle_ps(ZnXWY, XZYnW, _MM_SHUFFLE(2, 3, 0, 1)));

        /* now we only need to shuffle the components in place and return the result      */
        return _mm_shuffle_ps(XZWY, XZWY, _MM_SHUFFLE(2, 1, 3, 0));
    }
};

template <typename T>
Quaternion<T>::Quaternion() noexcept
{
    identity();
}

template <typename T>
Quaternion<T>::Quaternion(T x, T y, T z, T w) noexcept
    : _elements(x, y, z, w)
{
}

template <typename T>
Quaternion<T>::Quaternion(const vec4<T>& values) noexcept
    : _elements(values)
{
}

template <typename T>
Quaternion<T>::Quaternion(const mat3<T>& rotationMatrix) noexcept
{
    fromMatrix(rotationMatrix);
}

template <typename T>
Quaternion<T>::Quaternion(const vec3<T>& axis, Angle::DEGREES<T> angle) noexcept
{
    fromAxisAngle(axis, angle);
}

template <typename T>
Quaternion<T>::Quaternion(Angle::DEGREES<T> pitch, Angle::DEGREES<T> yaw, Angle::DEGREES<T> roll) noexcept
{
    fromEuler(pitch, yaw, roll);
}

template <typename T>
Quaternion<T>::Quaternion(const Quaternion& q) noexcept
    : _elements(q._elements)
{
}

template <typename T>
Quaternion<T>& Quaternion<T>::operator=(const Quaternion& q) {
    set(q);
    return *this;
}

template <typename T>
inline T Quaternion<T>::dot(const Quaternion<T>& rq) const {
    return _elements.dot(rq._elements);
}

template <typename T>
inline T Quaternion<T>::magnitude() const {
    return _elements.length();
}

template <typename T>
inline T Quaternion<T>::magnituteSq() const {
    return _elements.lengthSquared();
}

template <typename T>
inline bool Quaternion<T>::compare(const Quaternion<T>& rq, Angle::DEGREES<T> tolerance) const {
    T angleRad = Angle::to_RADIANS((T)std::acosf(to_F32(dot(rq))));
    const F32 toleranceRad = Angle::to_RADIANS(tolerance);

    return IS_TOLERANCE(angleRad, toleranceRad) || COMPARE_TOLERANCE(angleRad, to_F32(M_PI), toleranceRad);
}

template <typename T>
inline void Quaternion<T>::set(const vec4<T>& values) {
    _elements.set(values);
}

template <typename T>
inline void Quaternion<T>::set(T x, T y, T z, T w) {
    _elements.set(x, y, z, w);
}

template <typename T>
inline void Quaternion<T>::set(const Quaternion<T>& q) {
    set(q._elements);
}

template <typename T>
inline void Quaternion<T>::normalize() {
    _elements.normalize();
}

template <typename T>
inline Quaternion<T> Quaternion<T>::inverse() const {
    return getConjugate() * (1.0f / magnitude());
}

template <typename T>
inline Quaternion<T> Quaternion<T>::getConjugate() const {
    return Quaternion<T>(-X(), -Y(), -Z(), W());
}

template <typename T>
inline Quaternion<T> Quaternion<T>::operator*(const Quaternion<T>& rq) const {
    return Quaternion<T>(W() * rq.X() + X() * rq.W() + Y() * rq.Z() - Z() * rq.Y(),
                         W() * rq.Y() + Y() * rq.W() + Z() * rq.X() - X() * rq.Z(),
                         W() * rq.Z() + Z() * rq.W() + X() * rq.Y() - Y() * rq.X(),
                         W() * rq.W() - X() * rq.X() - Y() * rq.Y() - Z() * rq.Z());
}

template <>
inline Quaternion<F32> Quaternion<F32>::operator*(const Quaternion<F32>& rq) const {
    return Quaternion<F32>(multiplynew(_elements._reg._reg, rq._elements._reg._reg));
}

template <typename T>
inline Quaternion<T>& Quaternion<T>::operator*=(const Quaternion<T>& rq) {
    (*this) = (*this) * rq;
    return (*this);
}

template <typename T>
inline Quaternion<T> Quaternion<T>::operator/(const Quaternion& rq) const {
    return ((*this) * (rq.inverse()));
}

template <typename T>
inline Quaternion<T>& Quaternion<T>::operator/=(const Quaternion& rq) {
    (*this) = rq / (*this);
    return (*this);
}

template <typename T>
vec3<T> Quaternion<T>::operator*(const vec3<T>& vec) const {
    // nVidia SDK implementation
    vec3<T> uv = Cross(_elements.xyz(), vec);
    vec3<T> uuv = Cross(_elements.xyz(), uv);
    uv *= (W() * 2);

    return vec + uv + (uuv * 2);
}

template <typename T>
bool Quaternion<T>::operator==(const Quaternion<T>& rq) const {
    return compare(rq);
}

template <typename T>
bool Quaternion<T>::operator!=(const Quaternion<T>& rq) const {
    return !compare(rq);
}

template <typename T>
inline Quaternion<T>& Quaternion<T>::operator+=(const Quaternion<T>& rq) {
    _elements += rq._elements;
    return *this;
}

template <typename T>
inline Quaternion<T>& Quaternion<T>::operator-=(const Quaternion<T>& rq) {
    _elements -= rq._elements;
    return *this;
}

template <typename T>
inline Quaternion<T>& Quaternion<T>::operator*=(T scalar) {
    _elements *= scalar;
    return *this;
}

template <typename T>
inline Quaternion<T>& Quaternion<T>::operator/=(T scalar) {
    _elements /= scalar;
    return *this;
}

template <typename T>
inline Quaternion<T> Quaternion<T>::operator+(const Quaternion<T>& rq) const {
    Quaternion<T> tmp(*this);
    tmp += rq;
    return tmp;
}

template <typename T>
inline Quaternion<T> Quaternion<T>::operator-(const Quaternion<T>& rq) const {
    Quaternion<T> tmp(*this);
    tmp -= rq;
    return tmp;
}

template <typename T>
inline Quaternion<T> Quaternion<T>::operator*(T scalar) const {
    Quaternion<T> tmp(*this);
    tmp *= scalar;
    return tmp;
}

template <typename T>
inline Quaternion<T> Quaternion<T>::operator/(T scalar) const {
    Quaternion<T> tmp(*this);
    tmp /= scalar;
    return tmp;
}

template <typename T>
inline void Quaternion<T>::slerp(const Quaternion<T>& q, F32 t) {
    slerp(*this, q, t);
}

template <typename T>
void Quaternion<T>::slerp(const Quaternion<T>& q0, const Quaternion<T>& q1, F32 t) {
    F32 k0 = 0.0f, k1 = 0.0f;
    T cosomega = q0.dot(q1);
    Quaternion<T> q;
    if (cosomega < 0.0) {
        cosomega = -cosomega;
        q._elements.set(-q1._elements);
    } else {
        q._elements.set(q1._elements);
    }

    if (1.0 - cosomega > 1e-6) {
        const F32 omega = to_F32(std::acos(cosomega));
        const F32 sinomega = to_F32(std::sin(omega));
        k0 = to_F32(std::sin((1.0f - t) * omega) / sinomega);
        k1 = to_F32(std::sin(t * omega) / sinomega);
    } else {
        k0 = 1.0f - t;
        k1 = t;
    }
    _elements.set(q0._elements * k0 + q._elements * k1);
}

template <typename T>
void Quaternion<T>::fromAxisAngle(const vec3<T>& v, Angle::DEGREES<T> angle) {
    const Angle::RADIANS<T> angleHalfRad = Angle::to_RADIANS(angle) * 0.5f;
    _elements.set(Normalized(v) * std::sin(angleHalfRad), std::cos(angleHalfRad));
}

template <typename T>
inline void Quaternion<T>::fromEuler(const vec3<Angle::DEGREES<T>>& v) {
    fromEuler(v.pitch, v.yaw, v.roll);
}

template <typename T>
void Quaternion<T>::fromEuler(Angle::DEGREES<T> pitch, Angle::DEGREES<T> yaw, Angle::DEGREES<T> roll) {
    vec3<D64> eulerAngles(Angle::to_RADIANS(pitch) * 0.5, 
                          Angle::to_RADIANS(yaw)   * 0.5,
                          Angle::to_RADIANS(roll)  * 0.5);

    vec3<D64> c(std::cos(eulerAngles.x),
                std::cos(eulerAngles.y),
                std::cos(eulerAngles.z));
    vec3<D64> s(std::sin(eulerAngles.x),
                std::sin(eulerAngles.y),
                std::sin(eulerAngles.z));

    W(c.x * c.y * c.z + s.x * s.y * s.z);
    X(s.x * c.y * c.z - c.x * s.y * s.z);
    Y(c.x * s.y * c.z + s.x * c.y * s.z);
    Z(c.x * c.y * s.z - s.x * s.y * c.z);
    // normalize(); this method does produce a normalized quaternion
}

//ref: https://gamedev.stackexchange.com/questions/15070/orienting-a-model-to-face-a-target
template <typename T>
void Quaternion<T>::fromRotation(const vec3<T>& sourceDirection, const vec3<T>& destinationDirection, const vec3<T>& up) {
    F32 dot = to_F32(Dot(sourceDirection, destinationDirection));

    if (std::abs(dot - (-1.0f)) < EPSILON_F32) {
        // vector a and b point exactly in the opposite direction, 
        // so it is a 180 degrees turn around the up-axis
        fromAxisAngle(up, M_PI);
    } else if (std::abs(dot - (1.0f)) < EPSILON_F32) {
        // vector a and b point exactly in the same direction
        // so we return the identity quaternion
        identity();
    } else {
        fromAxisAngle(Normalized(Cross(sourceDirection, destinationDirection)), std::acos(dot));
    }
}

template <typename T>
void Quaternion<T>::fromMatrix(const mat4<T>& viewMatrix) {
    mat3<T> rotMatrix;
    viewMatrix.extractMat3(rotMatrix);
    fromMatrix(rotMatrix);
}

template <typename T>
void Quaternion<T>::fromMatrix(const mat3<T>& rotationMatrix) {
    // Algorithm in Ken Shoemake's article in 1987 SIGGRAPH course notes
    // article "Quaternion Calculus and Fast Animation".

    T fTrace = rotationMatrix.m[0][0] + rotationMatrix.m[1][1] +
               rotationMatrix.m[2][2];
    T fRoot = T(0);

    if (fTrace > T(0)) {
        // |w| > 1/2, may as well choose w > 1/2
        fRoot = Divide::Sqrt<T, F32>(fTrace + 1.0f);  // 2w
        W(T(0.5f * fRoot));
        fRoot = T(0.5f / fRoot);  // 1/(4w)
        X((rotationMatrix.m[2][1] - rotationMatrix.m[1][2]) * fRoot);
        Y((rotationMatrix.m[0][2] - rotationMatrix.m[2][0]) * fRoot);
        Z((rotationMatrix.m[1][0] - rotationMatrix.m[0][1]) * fRoot);
    } else {
        // |w| <= 1/2
        static size_t s_iNext[3] = {1, 2, 0};
        size_t i = 0;
        if (rotationMatrix.m[1][1] > rotationMatrix.m[0][0]) {
            i = 1;
        }
        if (rotationMatrix.m[2][2] > rotationMatrix.m[i][i]) {
            i = 2;
        }
        size_t j = s_iNext[i];
        size_t k = s_iNext[j];

        fRoot = Divide::Sqrt<T, F32>(rotationMatrix.m[i][i] - rotationMatrix.m[j][j] - rotationMatrix.m[k][k] + 1.0f);
        T* apkQuat[3] = {&_elements.x, &_elements.y, &_elements.z};
        *apkQuat[i] = T(0.5f * fRoot);
        fRoot = T(0.5f / fRoot);
        W((rotationMatrix.m[k][j] - rotationMatrix.m[j][k]) * fRoot);
        *apkQuat[j] = (rotationMatrix.m[j][i] + rotationMatrix.m[i][j]) * fRoot;
        *apkQuat[k] = (rotationMatrix.m[k][i] + rotationMatrix.m[i][k]) * fRoot;
    }
}

template <typename T>
void Quaternion<T>::getMatrix(mat3<T>& outMatrix) const {
    const T& x = X();
    const T& y = Y();
    const T& z = Z();
    const T& w = W();
    T fTx = x + x;
    T fTy = y + y;
    T fTz = z + z;
    T fTwx = fTx*w;
    T fTwy = fTy*w;
    T fTwz = fTz*w;
    T fTxx = fTx*x;
    T fTxy = fTy*x;
    T fTxz = fTz*x;
    T fTyy = fTy*y;
    T fTyz = fTz*y;
    T fTzz = fTz*z;

    outMatrix.m[0][0] = static_cast<T>(1.0f - (fTyy + fTzz));
    outMatrix.m[0][1] =         fTxy - fTwz;
    outMatrix.m[0][2] =         fTxz + fTwy;
    outMatrix.m[1][0] =         fTxy + fTwz;
    outMatrix.m[1][1] = static_cast<T>(1.0f - (fTxx + fTzz));
    outMatrix.m[1][2] =         fTyz - fTwx;
    outMatrix.m[2][0] =         fTxz - fTwy;
    outMatrix.m[2][1] =         fTyz + fTwx;
    outMatrix.m[2][2] = static_cast<T>(1.0f - (fTxx + fTyy));
}

template <typename T>
void Quaternion<T>::getAxisAngle(vec3<T>& axis, Angle::DEGREES<T>& angle) const {
    axis.set(_elements / _elements.xyz().length());
    angle = Angle::to_DEGREES(std::acos(W()) * 2.0f);
}

template <typename T>
void Quaternion<T>::getEuler(vec3<Angle::RADIANS<T>>& euler) const {
    T& yaw = euler.yaw;
    T& pitch = euler.pitch;
    T& roll = euler.roll;

    const T& x = X();
    const T& y = Y();
    const T& z = Z();
    const T& w = W();
    T sqx = x * x;
    T sqy = y * y;
    T sqz = z * z;
    T sqw = w * w;
    T test = x * y + z * w;
    // if normalized is one, otherwise is correction factor
    T unit = sqx + sqy + sqz + sqw;  

    if (test > (0.5f - EPSILON_F32) * unit) {  // singularity at north pole
        roll = 0;
        pitch = 2 * std::atan2(x, w);
        yaw = -static_cast<T>(M_PI2);
    } else if (test < -(0.5f - EPSILON_F32) * unit) {  // singularity at south pole
        roll = 0;
        pitch = -2 * std::atan2(x, w);
        yaw = static_cast<T>(M_PI2);
    } else {
        roll  = std::atan2(2 * x * y + 2 * w * z, sqw + sqx - sqy - sqz);
        pitch = std::atan2(2 * y * z + 2 * w * x, sqw - sqx - sqy + sqz);
        yaw   = std::asin(-2 * (x * z - w * y));
    }
}

template <typename T>
void Quaternion<T>::fromAxes(const vec3<T>* axis) {

    mat3<T> rot;
    for (U8 col = 0; col < 3; col++) {
        rot.setCol(iCol, axis[iCol]);
    }

    fromMatrix(rot);
}

template <typename T>
void Quaternion<T>::fromAxes(const vec3<T>& xAxis, const vec3<T>& yAxis, const vec3<T>& zAxis) {

    mat3<T> rot;
    
    rot.setCol(0, xAxis);
    rot.setCol(1, yAxis);
    rot.setCol(2, zAxis);

    fromMatrix(rot);
}

template <typename T>
void Quaternion<T>::toAxes(vec3<T>* axis) const {
    toAxes(axis[0], axis[1], axis[2]);
}

template <typename T>
void Quaternion<T>::toAxes(vec3<T>& xAxis, vec3<T>& yAxis, vec3<T>& zAxis) const {
    mat3<T> rot
    getMatrix(rot);
    xAxis.set(rot.getCol(0));
    yAxis.set(rot.getCol(1));
    zAxis.set(rot.getCol(2));
}

template <typename T>
vec3<T> Quaternion<T>::xAxis() const {
    const T& x = X();
    const T& y = Y();
    const T& z = Z();
    const T& w = W();

    //T fTx = 2.0f*x;
    T fTy = 2.0f*y;
    T fTz = 2.0f*z;
    T fTwy = fTy*w;
    T fTwz = fTz*w;
    T fTxy = fTy*x;
    T fTxz = fTz*x;
    T fTyy = fTy*y;
    T fTzz = fTz*z;

    return vec3<T>(1.0f - (fTyy + fTzz), fTxy + fTwz, fTxz - fTwy);
}

template <typename T>
vec3<T> Quaternion<T>::yAxis() const {
    const T& x = X();
    const T& y = Y();
    const T& z = Z();
    const T& w = W();

    T fTx = 2.0f*x;
    T fTy = 2.0f*y;
    T fTz = 2.0f*z;
    T fTwx = fTx*w;
    T fTwz = fTz*w;
    T fTxx = fTx*x;
    T fTxy = fTy*x;
    T fTyz = fTz*y;
    T fTzz = fTz*z;

    return vec3<T>(fTxy - fTwz, 1.0f - (fTxx + fTzz), fTyz + fTwx);
}

template <typename T>
vec3<T> Quaternion<T>::zAxis() const {
    const T& x = X();
    const T& y = Y();
    const T& z = Z();
    const T& w = W();

    T fTx = 2.0f*x;
    T fTy = 2.0f*y;
    T fTz = 2.0f*z;
    T fTwx = fTx*w;
    T fTwy = fTy*w;
    T fTxx = fTx*x;
    T fTxz = fTz*x;
    T fTyy = fTy*y;
    T fTyz = fTz*y;

    return vec3<T>(fTxz + fTwy, fTyz - fTwx, 1.0f - (fTxx + fTyy));
}

template <typename T>
inline T Quaternion<T>::X() const noexcept {
    return _elements.x;
}

template <typename T>
inline T Quaternion<T>::Y() const noexcept {
    return _elements.y;
}

template <typename T>
inline T Quaternion<T>::Z() const noexcept {
    return _elements.z;
}

template <typename T>
inline T Quaternion<T>::W() const noexcept {
    return _elements.w;
}

template <typename T>
inline vec3<T> Quaternion<T>::XYZ() const noexcept {
    return _elements.xyz();
}

template <typename T>
template <typename U>
inline void Quaternion<T>::X(U x) noexcept {
    _elements.x = static_cast<T>(x);
}

template <typename T>
template <typename U>
inline void Quaternion<T>::Y(U y) noexcept {
    _elements.y = static_cast<T>(y);
}

template <typename T>
template <typename U>
inline void Quaternion<T>::Z(U z) noexcept {
    _elements.z = static_cast<T>(z);
}

template <typename T>
template <typename U>
inline void Quaternion<T>::W(U w) noexcept {
    _elements.w = static_cast<T>(w);
}

template <typename T>
inline void Quaternion<T>::identity() {
    _elements.set(0, 0, 0, 1);
}

template <typename T>
inline const vec4<T>& Quaternion<T>::asVec4() const {
    return _elements;
}


/// get the shortest arc quaternion to rotate vector 'v' to the target vector
/// 'u'(from Ogre3D!)
template <typename T>
inline Quaternion<T> RotationFromVToU(const vec3<T>& v, const vec3<T>& u, const vec3<T>& fallbackAxis) {
    // Based on Stan Melax's article in Game Programming Gems
    Quaternion<T> q;
    // Copy, since cannot modify local
    vec3<T> v0 = v;
    vec3<T> v1 = u;
    v0.normalize();
    v1.normalize();

    T d = v0.dot(v1);
    // If dot == 1, vectors are the same
    if (d >= 1.0f) {
        return q;
    } else if (d < (1e-6f - 1.0f)) {
        if (!fallbackAxis.compare(VECTOR3_ZERO)) {
            // rotate 180 degrees about the fallback axis
            q.fromAxisAngle(fallbackAxis, to_F32(M_PI));
        } else {
            // Generate an axis
            vec3<T> axis;
            axis.cross(WORLD_X_AXIS, v);

            if (axis.isZeroLength())  // pick another if collinear
                axis.cross(WORLD_Y_AXIS, v);

            axis.normalize();
            q.fromAxisAngle(axis, to_F32(M_PI));
        }
    } else {
        F32 s = Divide::Sqrt((1 + d) * 2.0f);
        F32 invs = 1 / s;

        vec3<T> c(Cross(v0, v1) * invs);
        q.set(c.x, c.y, c.z, s * 0.5f);
        q.normalize();
    }

    return q;
}

template <typename T>
inline Quaternion<T> Slerp(const Quaternion<T>& q0, const Quaternion<T>& q1, F32 t) {
    Quaternion<T> temp;
    temp.slerp(q0, q1, t);
    return temp;
}

template <typename T>
inline mat3<T> GetMatrix(const Quaternion<T>& q) {
    mat3<T> temp;
    q.getMatrix(temp);
    return temp;
}

template <typename T>
inline vec3<Angle::RADIANS<T>> GetEuler(const Quaternion<T>& q) {
    vec3<Angle::RADIANS<T>> euler;
    q.getEuler(euler);
    return euler;
}

template <typename T>
inline vec3<T> operator*(vec3<T> const & v, Quaternion<T> const & q) {
    return q.inverse() * v;
}

template <typename T>
inline vec3<T> Rotate(vec3<T> const & v, Quaternion<T> const & q) {
    const vec3<T> xyz = q.XYZ();
    const vec3<T> t = Cross(xyz, v) * 2;
    return v + q.W() * t + Cross(xyz, t);
}

template <typename T>
inline vec3<T> DirectionFromAxis(const Quaternion<T>& q, const vec3<T>& AXIS) {
    return Normalized(Rotate(AXIS, q));
}

template <typename T>
inline vec3<T> DirectionFromEuler(vec3<Angle::DEGREES<T>> const & euler, const vec3<T>& FORWARD_DIRECTION) {
    Quaternion<F32> q = {};
    q.fromEuler(euler);
    return DirectionFromAxis(q, FORWARD_DIRECTION);
}
};  // namespace Divide

#endif  //_QUATERNION_INL_