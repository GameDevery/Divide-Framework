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

#ifndef _MATH_MATRICES_INL_
#define _MATH_MATRICES_INL_
namespace Divide {

#ifndef USE_AVX
#define USE_AVX
#endif

#define MakeShuffleMask(x,y,z,w)           (x | (y<<2) | (z<<4) | (w<<6))

    // vec(0, 1, 2, 3) -> (vec[x], vec[y], vec[z], vec[w])
#define VecSwizzle(vec, x,y,z,w)           _mm_shuffle_ps(vec, vec, MakeShuffleMask(x,y,z,w))
#define VecSwizzle1(vec, x)                _mm_shuffle_ps(vec, vec, MakeShuffleMask(x,x,x,x))
    // special swizzle
#define VecSwizzle_0101(vec)               _mm_movelh_ps(vec, vec)
#define VecSwizzle_2323(vec)               _mm_movehl_ps(vec, vec)
#define VecSwizzle_0022(vec)               _mm_moveldup_ps(vec)
#define VecSwizzle_1133(vec)               _mm_movehdup_ps(vec)

    // return (vec1[x], vec1[y], vec2[z], vec2[w])
#define VecShuffle(vec1, vec2, x,y,z,w)    _mm_shuffle_ps(vec1, vec2, MakeShuffleMask(x,y,z,w))
    // special shuffle
#define VecShuffle_0101(vec1, vec2)        _mm_movelh_ps(vec1, vec2)
#define VecShuffle_2323(vec1, vec2)        _mm_movehl_ps(vec2, vec1)

#define SMALL_NUMBER		(1.e-8f)

namespace {
    //Ref: http://stackoverflow.com/questions/18499971/efficient-4x4-matrix-multiplication-c-vs-assembly
    void M4x4_SSE(const F32 *A, const F32 *B, F32 *C) noexcept {
        const __m128 row1 = _mm_load_ps(&B[0]);
        const __m128 row2 = _mm_load_ps(&B[4]);
        const __m128 row3 = _mm_load_ps(&B[8]);
        const __m128 row4 = _mm_load_ps(&B[12]);
        for (U8 i = 0; i < 4; ++i) {
            const __m128 brod1 = _mm_set1_ps(A[4 * i + 0]);
            const __m128 brod2 = _mm_set1_ps(A[4 * i + 1]);
            const __m128 brod3 = _mm_set1_ps(A[4 * i + 2]);
            const __m128 brod4 = _mm_set1_ps(A[4 * i + 3]);
            const __m128 row = _mm_add_ps(_mm_add_ps(_mm_mul_ps(brod1, row1),
                                                     _mm_mul_ps(brod2, row2)),
                                          _mm_add_ps(_mm_mul_ps(brod3, row3),
                                                     _mm_mul_ps(brod4, row4)));
           _mm_store_ps(&C[4 * i], row);
        }
    }

    //ref: https://gist.github.com/rygorous/4172889
#ifndef USE_AVX
    // linear combination:
    // a[0] * B.row[0] + a[1] * B.row[1] + a[2] * B.row[2] + a[3] * B.row[3]
    static inline __m128 lincomb_SSE(const __m128 &a, const mat4<F32> &B)
    {
        __m128 result;
        result = _mm_mul_ps(_mm_shuffle_ps(a, a, 0x00), B._reg[0]._reg);
        result = _mm_add_ps(result, _mm_mul_ps(_mm_shuffle_ps(a, a, 0x55), B._reg[1]._reg));
        result = _mm_add_ps(result, _mm_mul_ps(_mm_shuffle_ps(a, a, 0xaa), B._reg[2]._reg));
        result = _mm_add_ps(result, _mm_mul_ps(_mm_shuffle_ps(a, a, 0xff), B._reg[3]._reg));
        return result;
    }

    // this is the right approach for SSE ... SSE4.2
    void M4x4_SSE(const mat4<F32>& A, const mat4<F32>& B, mat4<F32>& C)
    {
        // out_ij = sum_k a_ik b_kj
        // => out_0j = a_00 * b_0j + a_01 * b_1j + a_02 * b_2j + a_03 * b_3j
        __m128 out0x = lincomb_SSE(A._reg[0]._reg, B);
        __m128 out1x = lincomb_SSE(A._reg[1]._reg, B);
        __m128 out2x = lincomb_SSE(A._reg[2]._reg, B);
        __m128 out3x = lincomb_SSE(A._reg[3]._reg, B);

        C._reg[0]._reg = out0x;
        C._reg[1]._reg = out1x;
        C._reg[2]._reg = out2x;
        C._reg[3]._reg = out3x;
    }
#else
    // another linear combination, using AVX instructions on XMM regs
    static inline __m128 lincomb_AVX_4mem(const F32 *a, const mat4<F32> &B)
    {
        __m128 result;
        result = _mm_mul_ps(_mm_broadcast_ss(&a[0]), B._reg[0]._reg);
        result = _mm_add_ps(result, _mm_mul_ps(_mm_broadcast_ss(&a[1]), B._reg[1]._reg));
        result = _mm_add_ps(result, _mm_mul_ps(_mm_broadcast_ss(&a[2]), B._reg[2]._reg));
        result = _mm_add_ps(result, _mm_mul_ps(_mm_broadcast_ss(&a[3]), B._reg[3]._reg));
        return result;
    }

    // using AVX instructions, 4-wide
    // this can be better if A is in memory.
    void M4x4_SSE(const mat4<F32> &A, const mat4<F32> &B, mat4<F32>& C)
    {
        _mm256_zeroupper();
        __m128 out0x = lincomb_AVX_4mem(A.m[0], B);
        __m128 out1x = lincomb_AVX_4mem(A.m[1], B);
        __m128 out2x = lincomb_AVX_4mem(A.m[2], B);
        __m128 out3x = lincomb_AVX_4mem(A.m[3], B);

        C._reg[0] = out0x;
        C._reg[1] = out1x;
        C._reg[2] = out2x;
        C._reg[3] = out3x;
    }

#endif

    //ref: https://lxjk.github.io/2017/09/03/Fast-4x4-Matrix-Inverse-with-SSE-SIMD-Explained.html
    inline void GetTransformInverseNoScale(const mat4<F32>& inM, mat4<F32>& r)
    {
        // transpose 3x3, we know m03 = m13 = m23 = 0
        __m128 t0 = VecShuffle_0101(inM._reg[0]._reg, inM._reg[1]._reg); // 00, 01, 10, 11
        __m128 t1 = VecShuffle_2323(inM._reg[0]._reg, inM._reg[1]._reg); // 02, 03, 12, 13
        r._reg[0] = VecShuffle(t0, inM._reg[2]._reg, 0, 2, 0, 3);        // 00, 10, 20, 23(=0)
        r._reg[1] = VecShuffle(t0, inM._reg[2]._reg, 1, 3, 1, 3);        // 01, 11, 21, 23(=0)
        r._reg[2] = VecShuffle(t1, inM._reg[2]._reg, 0, 2, 2, 3);        // 02, 12, 22, 23(=0)

                                                             // last line
        r._reg[3] = _mm_mul_ps(r._reg[0]._reg, VecSwizzle1(inM._reg[3]._reg, 0));
        r._reg[3] = _mm_add_ps(r._reg[3]._reg, _mm_mul_ps(r._reg[1]._reg, VecSwizzle1(inM._reg[3]._reg, 1)));
        r._reg[3] = _mm_add_ps(r._reg[3]._reg, _mm_mul_ps(r._reg[2]._reg, VecSwizzle1(inM._reg[3]._reg, 2)));
        r._reg[3] = _mm_sub_ps(_mm_setr_ps(0.f, 0.f, 0.f, 1.f), r._reg[3]._reg);
    }

    // for column major matrix
    // we use __m128 to represent 2x2 matrix as A = | A0  A2 |
    //                                              | A1  A3 |
    // 2x2 column major Matrix multiply A*B
    __forceinline __m128 Mat2Mul(__m128 vec1, __m128 vec2)
    {
        return  _mm_add_ps(_mm_mul_ps(vec1, VecSwizzle(vec2, 0, 0, 3, 3)),
                           _mm_mul_ps(VecSwizzle(vec1, 2, 3, 0, 1), VecSwizzle(vec2, 1, 1, 2, 2)));
    }
    // 2x2 column major Matrix adjugate multiply (A#)*B
    __forceinline __m128 Mat2AdjMul(__m128 vec1, __m128 vec2)
    {
        return  _mm_sub_ps(_mm_mul_ps(VecSwizzle(vec1, 3, 0, 3, 0), vec2),
                            _mm_mul_ps(VecSwizzle(vec1, 2, 1, 2, 1), VecSwizzle(vec2, 1, 0, 3, 2)));

    }
    // 2x2 column major Matrix multiply adjugate A*(B#)
    __forceinline __m128 Mat2MulAdj(__m128 vec1, __m128 vec2)
    {
        return  _mm_sub_ps(_mm_mul_ps(vec1, VecSwizzle(vec2, 3, 3, 0, 0)),
                           _mm_mul_ps(VecSwizzle(vec1, 2, 3, 0, 1), VecSwizzle(vec2, 1, 1, 2, 2)));
    }

    // Requires this matrix to be transform matrix
    inline void GetTransformInverse(const mat4<F32>& inM, mat4<F32>& r)
    {
        // transpose 3x3, we know m03 = m13 = m23 = 0
        __m128 t0 = VecShuffle_0101(inM._reg[0]._reg, inM._reg[1]._reg); // 00, 01, 10, 11
        __m128 t1 = VecShuffle_2323(inM._reg[0]._reg, inM._reg[1]._reg); // 02, 03, 12, 13
        r._reg[0]._reg = VecShuffle(t0, inM._reg[2]._reg, 0, 2, 0, 3);   // 00, 10, 20, 23(=0)
        r._reg[1]._reg = VecShuffle(t0, inM._reg[2]._reg, 1, 3, 1, 3);   // 01, 11, 21, 23(=0)
        r._reg[2]._reg = VecShuffle(t1, inM._reg[2]._reg, 0, 2, 2, 3);   // 02, 12, 22, 23(=0)

        // (SizeSqr(_reg[0]._reg), SizeSqr(_reg[1]._reg), SizeSqr(_reg[2]._reg), 0)
        __m128 sizeSqr;
        sizeSqr = _mm_mul_ps(r._reg[0]._reg, r._reg[0]._reg);
        sizeSqr = _mm_add_ps(sizeSqr, _mm_mul_ps(r._reg[1]._reg, r._reg[1]._reg));
        sizeSqr = _mm_add_ps(sizeSqr, _mm_mul_ps(r._reg[2]._reg, r._reg[2]._reg));

        // optional test to avoid divide by 0
        __m128 one = _mm_set1_ps(1.f);
        // for each component, if(sizeSqr < SMALL_NUMBER) sizeSqr = 1;
        __m128 rSizeSqr = _mm_blendv_ps(
            _mm_div_ps(one, sizeSqr),
            one,
            _mm_cmplt_ps(sizeSqr, _mm_set1_ps(SMALL_NUMBER))
        );

        r._reg[0] = _mm_mul_ps(r._reg[0]._reg, rSizeSqr);
        r._reg[1] = _mm_mul_ps(r._reg[1]._reg, rSizeSqr);
        r._reg[2] = _mm_mul_ps(r._reg[2]._reg, rSizeSqr);

        // last line
        r._reg[3] = _mm_mul_ps(r._reg[0]._reg, VecSwizzle1(inM._reg[3]._reg, 0));
        r._reg[3] = _mm_add_ps(r._reg[3]._reg, _mm_mul_ps(r._reg[1]._reg, VecSwizzle1(inM._reg[3]._reg, 1)));
        r._reg[3] = _mm_add_ps(r._reg[3]._reg, _mm_mul_ps(r._reg[2]._reg, VecSwizzle1(inM._reg[3]._reg, 2)));
        r._reg[3] = _mm_sub_ps(_mm_setr_ps(0.f, 0.f, 0.f, 1.f), r._reg[3]._reg);
    }

    // Inverse function is the same no matter column major or row major
    // this version treats it as column major
    inline void GetInverse(const mat4<F32>& inM, mat4<F32>& r)
    {
        // use block matrix method
        // A is a matrix, then i(A) or iA means inverse of A, A# (or A_ in code) means adjugate of A, |A| (or detA in code) is determinant, tr(A) is trace

        // sub matrices
        __m128 A = VecShuffle_0101(inM._reg[0]._reg, inM._reg[1]._reg);
        __m128 C = VecShuffle_2323(inM._reg[0]._reg, inM._reg[1]._reg);
        __m128 B = VecShuffle_0101(inM._reg[2]._reg, inM._reg[3]._reg);
        __m128 D = VecShuffle_2323(inM._reg[2]._reg, inM._reg[3]._reg);

        __m128 detA = _mm_set1_ps(inM.m[0][0] * inM.m[1][1] - inM.m[0][1] * inM.m[1][0]);
        __m128 detC = _mm_set1_ps(inM.m[0][2] * inM.m[1][3] - inM.m[0][3] * inM.m[1][2]);
        __m128 detB = _mm_set1_ps(inM.m[2][0] * inM.m[3][1] - inM.m[2][1] * inM.m[3][0]);
        __m128 detD = _mm_set1_ps(inM.m[2][2] * inM.m[3][3] - inM.m[2][3] * inM.m[3][2]);

#if 0 // for determinant, float version is faster
        // determinant as (|A| |C| |B| |D|)
        __m128 detSub = _mm_sub_ps(
            _mm_mul_ps(VecShuffle(inM._reg[0]._reg, inM._reg[2]._reg, 0, 2, 0, 2), VecShuffle(inM._reg[1]._reg, inM._reg[3]._reg, 1, 3, 1, 3)),
            _mm_mul_ps(VecShuffle(inM._reg[0]._reg, inM._reg[2]._reg, 1, 3, 1, 3), VecShuffle(inM._reg[1]._reg, inM._reg[3]._reg, 0, 2, 0, 2))
        );
        __m128 detA = VecSwizzle1(detSub, 0);
        __m128 detC = VecSwizzle1(detSub, 1);
        __m128 detB = VecSwizzle1(detSub, 2);
        __m128 detD = VecSwizzle1(detSub, 3);
#endif

        // let iM = 1/|M| * | X  Y |
        //                  | Z  W |

        // D#C
        __m128 D_C = Mat2AdjMul(D, C);
        // A#B
        __m128 A_B = Mat2AdjMul(A, B);
        // X# = |D|A - B(D#C)
        __m128 X_ = _mm_sub_ps(_mm_mul_ps(detD, A), Mat2Mul(B, D_C));
        // W# = |A|D - C(A#B)
        __m128 W_ = _mm_sub_ps(_mm_mul_ps(detA, D), Mat2Mul(C, A_B));

        // |M| = |A|*|D| + ... (continue later)
        __m128 detM = _mm_mul_ps(detA, detD);

        // Y# = |B|C - D(A#B)#
        __m128 Y_ = _mm_sub_ps(_mm_mul_ps(detB, C), Mat2MulAdj(D, A_B));
        // Z# = |C|B - A(D#C)#
        __m128 Z_ = _mm_sub_ps(_mm_mul_ps(detC, B), Mat2MulAdj(A, D_C));

        // |M| = |A|*|D| + |B|*|C| ... (continue later)
        detM = _mm_add_ps(detM, _mm_mul_ps(detB, detC));

        // tr((A#B)(D#C))
        __m128 tr = _mm_mul_ps(A_B, VecSwizzle(D_C, 0, 2, 1, 3));
        tr = _mm_hadd_ps(tr, tr);
        tr = _mm_hadd_ps(tr, tr);
        // |M| = |A|*|D| + |B|*|C| - tr((A#B)(D#C))
        detM = _mm_sub_ps(detM, tr);

        const __m128 adjSignMask = _mm_setr_ps(1.f, -1.f, -1.f, 1.f);
        // (1/|M|, -1/|M|, -1/|M|, 1/|M|)
        __m128 rDetM = _mm_div_ps(adjSignMask, detM);

        X_ = _mm_mul_ps(X_, rDetM);
        Y_ = _mm_mul_ps(Y_, rDetM);
        Z_ = _mm_mul_ps(Z_, rDetM);
        W_ = _mm_mul_ps(W_, rDetM);

        // apply adjugate and store, here we combine adjugate shuffle and store shuffle
        r._reg[0]._reg = VecShuffle(X_, Z_, 3, 1, 3, 1);
        r._reg[1]._reg = VecShuffle(X_, Z_, 2, 0, 2, 0);
        r._reg[2]._reg = VecShuffle(Y_, W_, 3, 1, 3, 1);
        r._reg[3]._reg = VecShuffle(Y_, W_, 2, 0, 2, 0);
    }

    inline mat4<F32> GetInverse(const mat4<F32>& inM) {
        mat4<F32> r;
        GetInverse(inM, r);
        return r;
    }
};

/*********************************
* mat2
*********************************/
template<typename T>
mat2<T>::mat2() noexcept
    : mat2(1, 0,
           0, 1)
{
}

template<typename T>
template<typename U>
mat2<T>::mat2(U m) noexcept
    : mat{ static_cast<T>(m), static_cast<T>(m),
           static_cast<T>(m), static_cast<T>(m) }
{
}

template<typename T>
template<typename U>
mat2<T>::mat2(U m0, U m1,
              U m2, U m3) noexcept
    : mat{static_cast<T>(m0), static_cast<T>(m1),
          static_cast<T>(m2), static_cast<T>(m3)}
{
}

template<typename T>
template<typename U>
mat2<T>::mat2(const U *values) noexcept
    : mat2(values[0], values[1],
           values[2], values[3])
{
}

template<typename T>
mat2<T>::mat2(const mat2 &B) noexcept
    : mat2(B.mat)
{
}

template<typename T>
template<typename U>
mat2<T>::mat2(const mat2<U> &B) noexcept
    : mat2(B.mat)
{
}

template<typename T>
template<typename U>
mat2<T>::mat2(const mat3<U> &B) noexcept
    : mat2(B[0], B[1],
           B[3], B[4])
{
}

template<typename T>
template<typename U>
mat2<T>::mat2(const mat4<U> &B) noexcept
    : mat2(B[0], B[1],
           B[4], B[5])
{
}

template<typename T>
mat2<T>& mat2<T>::operator=(const mat2& other) noexcept {
    set(other);
    return *this;
}

template<typename T>
template<typename U>
mat2<T>& mat2<T>::operator=(const mat2<U>& other) noexcept {
    set(other);
    return *this;
}

template<typename T>
template<typename U>
vec2<T> mat2<T>::operator*(const vec2<U> &v) const {
    return vec2<T>(mat[0] * v[0] + mat[1] * v[1],
                   mat[2] * v[0] + mat[3] * v[1]);
}

template<typename T>
template<typename U>
vec3<T> mat2<T>::operator*(const vec3<U> &v) const {
    return vec3<T>(mat[0] * v[0] + mat[1] * v[1],
                   mat[2] * v[0] + mat[3] * v[1],
                   v[2]);
}

template<typename T>
template<typename U>
vec4<T> mat2<T>::operator*(const vec4<U> &v) const {
    return vec4<T>(mat[0] * v[0] + mat[1] * v[1],
                   mat[2] * v[0] + mat[3] * v[1],
                   v[2],
                   v[3]);
}

template<typename T>
template<typename U>
mat2<T> mat2<T>::operator*(const mat2<U> &B) const {
    return mat2(mat[0] * B.mat[0] + mat[1] * B.mat[2], mat[0] * B.mat[1] + mat[1] * B.mat[3],
                mat[2] * B.mat[0] + mat[3] * B.mat[2], mat[2] * B.mat[1] + mat[3] * B.mat[3]);
}

template<typename T>
template<typename U>
mat2<T> mat2<T>::operator/(const mat2<U> &B) const {
    return this * B.getInverse();
}

template<typename T>
template<typename U>
mat2<T> mat2<T>::operator+(const mat2<U> &B) const {
    return mat2(mat[0] + B[0], mat[1] + B[1],
                mat[2] + B[2], mat[3] + B[3]);
}

template<typename T>
template<typename U>
mat2<T> mat2<T>::operator-(const mat2<U> &B) const {
    return mat2(mat[0] - B[0], mat[1] - B[1],
                mat[2] - B[2], mat[3] - B[3]);
}

template<typename T>
template<typename U>
mat2<T>& mat2<T>::operator*=(const mat2<U> &B) {
    return *this = *this * B;
}

template<typename T>
template<typename U>
mat2<T>& mat2<T>::operator/=(const mat2<U> &B) {
    return *this = *this * B;
}

template<typename T>
template<typename U>
mat2<T>& mat2<T>::operator+=(const mat2<U> &B) {
    return *this = *this + B;
}

template<typename T>
template<typename U>
mat2<T>& mat2<T>::operator-=(const mat2<U> &B) {
    for (auto& val : _vec) {
        val -= f;
    }
    return *this;
}

template<typename T>
template<typename U>
mat2<T> mat2<T>::operator*(U f) const {
    return (mat2(*this) *= f);
}

template<typename T>
template<typename U>
mat2<T> mat2<T>::operator/(U f) const {
    return (mat2(*this) /= f);
}

template<typename T>
template<typename U>
mat2<T> mat2<T>::operator+(U f) const {
    return (mat2(*this) += f);
}

template<typename T>
template<typename U>
mat2<T> mat2<T>::operator-(U f) const {
    return (mat2(*this) -= f);
}

template<typename T>
template<typename U>
mat2<T>& mat2<T>::operator*=(U f) {
    for (auto& val : _vec) {
        val *= f;
    }
    return *this;
}

template<typename T>
template<typename U>
mat2<T>& mat2<T>::operator/=(U f) {
    for (auto& val : _vec) {
        val /= f;
    }
    return *this;
}

template<typename T>
template<typename U>
mat2<T>& mat2<T>::operator+=(U f) {
    for (auto& val : _vec) {
        val += f;
    }
    return *this;
}

template<typename T>
template<typename U>
mat2<T>& mat2<T>::operator-=(U f) {
    for (auto& val : _vec) {
        val -= f;
    }
    return *this;
}

template<typename T>
bool mat2<T>::operator==(const mat2 &B) const {
    for (U8 i = 0; i < 4; ++i) {
        if (!COMPARE(mat[i], B.mat[i])) {
            return false;
        }
    }
    return true;
}

template<typename T>
bool mat2<T>::operator!=(const mat2 &B) const {
    for (U8 i = 0; i < 4; ++i) {
        if (!COMPARE(mat[i], B.mat[i])) {
            return true;
        }
    }
    return false;
}

template<typename T>
template<typename U>
bool mat2<T>::operator==(const mat2<U> &B) const {
    for (U8 i = 0; i < 4; ++i) {
        if (!COMPARE(mat[i], B.mat[i])) {
            return false;
        }
    }
    return true;
}

template<typename T>
template<typename U>
bool mat2<T>::operator!=(const mat2<U> &B) const {
    for (U8 i = 0; i < 4; ++i) {
        if (!COMPARE(mat[i], B.mat[i])) {
            return true;
        }
    }
    return false;
}

template<typename T>
bool mat2<T>::compare(const mat2 &B, F32 epsilon) const {
    for (U8 i = 0; i < 4; ++i) {
        if (!COMPARE_TOLERANCE(mat[i], B.mat[i], epsilon)) {
            return false;
        }
    }
    return true;
}

template<typename T>
template<typename U>
bool mat2<T>::compare(const mat2<U> &B, F32 epsilon) const {
    for (U8 i = 0; i < 4; ++i) {
        if (!COMPARE_TOLERANCE(mat[i], B.mat[i], epsilon)) {
            return false;
        }
    }
    return true;
}

template<typename T>
mat2<T>::operator T *() {
    return mat;
}

template<typename T>
mat2<T>::operator const T *() const {
    return mat;
}

template<typename T>
T& mat2<T>::operator[](I32 i) {
    return mat[i];
}

template<typename T>
const T mat2<T>::operator[](I32 i) const {
    return mat[i];
}

template<typename T>
T& mat2<T>::element(I8 row, I8 column) {
    return m[row][column];
}

template<typename T>
const T& mat2<T>::element(I8 row, I8 column) const {
    return m[row][column];
}

template<typename T>
template<typename U>
void mat2<T>::set(U m0, U m1, U m2, U m3) {
    mat[0] = static_cast<T>(m0);
    mat[3] = static_cast<T>(m3);
    mat[1] = static_cast<T>(m1);
    mat[2] = static_cast<T>(m2);
}

template<typename T>
template<typename U>
void mat2<T>::set(const U *matrix) {
    if(sizeof(T) == sizeof(U)) {
        memcpy(mat, matrix, sizeof(U) * 4);
    } else {
        set(matrix[0], matrix[1],
            matrix[2], matrix[3]);
    }
}

template<typename T>
template<typename U>
void mat2<T>::set(const mat2<U> &matrix) {
    set(matrix.mat);
}

template<typename T>
template<typename U>
void mat2<T>::set(const mat3<U> &matrix) {
    set(matrix[0], matrix[1], matrix[3], matrix[4]);
}

template<typename T>
template<typename U>
void mat2<T>::set(const mat4<U> &matrix) {
    set(matrix[0], matrix[1], matrix[4], matrix[5]);
}

template<typename T>
template<typename U>
void mat2<T>::setRow(I32 index, const U value) {
    _vec[index].set(value);
}

template<typename T>
template<typename U>
void mat2<T>::setRow(I32 index, const vec2<U> &value) {
    _vec[index].set(value);
}

template<typename T>
template<typename U>
void mat2<T>::setRow(I32 index, const U x, const U y) {
    _vec[index].set(x, y);
}

template<typename T>
const vec2<T>& mat2<T>::getRow(I32 index) const {
    return _vec[index];
}

template<typename T>
template<typename U>
void mat2<T>::setCol(I32 index, const vec2<U> &value) {
    m[0][index] = static_cast<T>(value.x);
    m[1][index] = static_cast<T>(value.y);
}

template<typename T>
template<typename U>
void mat2<T>::setCol(I32 index, const U value) {
    m[0][index] = static_cast<T>(value);
    m[1][index] = static_cast<T>(value);
}

template<typename T>
template<typename U>
void mat2<T>::setCol(I32 index, const U x, const U y) {
    m[0][index] = static_cast<T>(x);
    m[1][index] = static_cast<T>(y);
}

template<typename T>
vec2<T> mat2<T>::getCol(I32 index) const {
    return vec2<T>(m[0][index], m[1][index]);
}

template<typename T>
void mat2<T>::zero() {
    memset(mat, 0, 4 * sizeof(T));
}

template<typename T>
void mat2<T>::identity() {
    mat[0] = static_cast<T>(1);
    mat[1] = static_cast<T>(0);
    mat[2] = static_cast<T>(0);
    mat[3] = static_cast<T>(1);
}

template<typename T>
bool mat2<T>::isIdentity() const {
    return (COMPARE(mat[0], 1) && IS_ZERO(mat[1]) &&
            IS_ZERO(mat[2])    && COMPARE(mat[3], 1));
}

template<typename T>
void mat2<T>::swap(mat2 &B) {
    std::swap(m[0][0], B.m[0][0]);
    std::swap(m[0][1], B.m[0][1]);

    std::swap(m[1][0], B.m[1][0]);
    std::swap(m[1][1], B.m[1][1]);
}

template<typename T>
T mat2<T>::det() const {
    return (mat[0] * mat[3] - mat[1] * mat[2]);
}

template<typename T>
T mat2<T>::elementSum() const {
    return mat[0] + mat[1] + mat[2] + mat[3];
}

template<typename T>
void mat2<T>::inverse() {
    F32 idet = static_cast<F32>(det());
    assert(!IS_ZERO(idet));
    idet = 1 / idet;

    set( mat[3] * idet, -mat[1] * idet,
        -mat[2] * idet,  mat[0] * idet);
}

template<typename T>
void mat2<T>::transpose() {
    set(mat[0], mat[2],
        mat[1], mat[3]);
}

template<typename T>
void mat2<T>::inverseTranspose() {
    inverse();
    transpose();
}

template<typename T>
mat2<T> mat2<T>::getInverse() const {
    mat2<T> ret(mat);
    ret.inverse();
    return ret;
}

template<typename T>
void mat2<T>::getInverse(mat2<T> &ret) const {
    ret.set(mat);
    ret.inverse();
}

template<typename T>
mat2<T> mat2<T>::getTranspose() const {
    mat2 ret(mat);
    ret.transpose();
    return ret;
}

template<typename T>
void mat2<T>::getTranspose(mat2 &ret) const {
    ret.set(mat);
    ret.transpose();
}

template<typename T>
mat2<T> mat2<T>::getInverseTranspose() const {
    mat2 ret;
    getInverse(ret);
    ret.transpose();
    return ret;
}

template<typename T>
void mat2<T>::getInverseTranspose(mat2 &ret) const {
    getInverse(ret);
    ret.transpose();
}


/*********************************
* mat3
*********************************/

template<typename T>
mat3<T>::mat3() noexcept
    : mat3(1, 0, 0,
           0, 1, 0,
           0, 0, 1)
{
}

template<typename T>
template<typename U>
mat3<T>::mat3(U m) noexcept
    : mat{ static_cast<T>(m), static_cast<T>(m), static_cast<T>(m),
           static_cast<T>(m), static_cast<T>(m), static_cast<T>(m),
           static_cast<T>(m), static_cast<T>(m), static_cast<T>(m) }
{
}

template<typename T>
template<typename U>
mat3<T>::mat3(U m0, U m1, U m2,
              U m3, U m4, U m5,
              U m6, U m7, U m8) noexcept
    : mat{static_cast<T>(m0), static_cast<T>(m1), static_cast<T>(m2),
          static_cast<T>(m3), static_cast<T>(m4), static_cast<T>(m5),
          static_cast<T>(m6), static_cast<T>(m7), static_cast<T>(m8)}
{
}

template<typename T>
template<typename U>
mat3<T>::mat3(const U *values) noexcept
    : mat3(values[0], values[1], values[2],
           values[3], values[4], values[5],
           values[6], values[7], values[8])
{
}

template<typename T>
template<typename U>
mat3<T>::mat3(const mat2<U> &B, bool zeroFill) noexcept
    : mat3(B[0], B[1], 0,
           B[2], B[3], 0,
           0,    0,    (zeroFill ? 0 : 1))
{
}

template<typename T>
mat3<T>::mat3(const mat3 &B) noexcept
    : mat3(B.mat)
{
}

template<typename T>
template<typename U>
mat3<T>::mat3(const mat3<U> &B) noexcept
    : mat3(B.mat)
{
}

template<typename T>
template<typename U>
mat3<T>::mat3(const mat4<U> &B) noexcept
    : mat3(B[0], B[1], B[2],
           B[4], B[5], B[6],
           B[8], B[9], B[10])
{
}

template<typename T>
mat3<T>& mat3<T>::operator=(const mat3& other) noexcept {
    set(other);
    return *this;
}

template<typename T>
template<typename U>
mat3<T>& mat3<T>::operator=(const mat3<U>& other) noexcept {
    set(other);
    return *this;
}

template<typename T>
template<typename U>
vec2<U> mat3<T>::operator*(const vec2<U> &v) const {
    return *this * vec3<U>(v);
}

template<typename T>
template<typename U>
vec3<U> mat3<T>::operator*(const vec3<U> &v) const {
    return vec3<U>(mat[0] * v[0] + mat[3] * v[1] + mat[6] * v[2],
                   mat[1] * v[0] + mat[4] * v[1] + mat[7] * v[2],
                   mat[2] * v[0] + mat[5] * v[1] + mat[8] * v[2]);
}

template<typename T>
template<typename U>
vec4<U> mat3<T>::operator*(const vec4<U> &v) const {
    return vec4<U>(mat[0] * v[0] + mat[3] * v[1] + mat[6] * v[2],
                   mat[1] * v[0] + mat[4] * v[1] + mat[7] * v[2],
                   mat[2] * v[0] + mat[5] * v[1] + mat[8] * v[2],
                   v[3]);
}

template<typename T>
template<typename U>
mat3<T> mat3<T>::operator*(const mat3<U> &B) const {
    return mat3(B.m[0][0] * m[0][0] + B.m[1][0] * m[0][1] + B.m[2][0] * m[0][2], B.m[0][1] * m[0][0] + B.m[1][1] * m[0][1] + B.m[2][1] * m[0][2], B.m[0][2] * m[0][0] + B.m[1][2] * m[0][1] + B.m[2][2] * m[0][2],
                B.m[0][0] * m[1][0] + B.m[1][0] * m[1][1] + B.m[2][0] * m[1][2], B.m[0][1] * m[1][0] + B.m[1][1] * m[1][1] + B.m[2][1] * m[1][2], B.m[0][2] * m[1][0] + B.m[1][2] * m[1][1] + B.m[2][2] * m[1][2],
                B.m[0][0] * m[2][0] + B.m[1][0] * m[2][1] + B.m[2][0] * m[2][2], B.m[0][1] * m[2][0] + B.m[1][1] * m[2][1] + B.m[2][1] * m[2][2], B.m[0][2] * m[2][0] + B.m[1][2] * m[2][1] + B.m[2][2] * m[2][2]);
}

template<typename T>
template<typename U>
mat3<T> mat3<T>::operator/(const mat3<U> &B) const {
    return *this * B.getInverse();
}

template<typename T>
template<typename U>
mat3<T> mat3<T>::operator+(const mat3<U> &B) const {
    return mat3(mat[0] + B[0], mat[1] + B[1], mat[2] + B[2],
                mat[3] + B[3], mat[4] + B[4], mat[5] + B[5],
                mat[6] + B[6], mat[7] + B[7], mat[8] + B[8]);
}

template<typename T>
template<typename U>
mat3<T> mat3<T>::operator-(const mat3<U> &B) const {
    return mat3(mat[0] - B[0], mat[1] - B[1], mat[2] - B[2],
                mat[3] - B[3], mat[4] - B[4], mat[5] - B[5],
                mat[6] - B[6], mat[7] - B[7], mat[8] - B[8]);
}

template<typename T>
template<typename U>
mat3<T>& mat3<T>::operator*=(const mat3<U> &B) {
    return *this = *this * B;
}

template<typename T>
template<typename U>
mat3<T>& mat3<T>::operator/=(const mat3<U> &B) {
    return *this = *this / B;
}

template<typename T>
template<typename U>
mat3<T>& mat3<T>::operator+=(const mat3<U> &B) {
    return *this = *this + B;
}

template<typename T>
template<typename U>
mat3<T>& mat3<T>::operator-=(const mat3<U> &B) {
    return *this = *this - B;
}

template<typename T>
template<typename U>
mat3<T> mat3<T>::operator*(U f) const {
    return (mat3(*this) *= f);
}

template<typename T>
template<typename U>
mat3<T> mat3<T>::operator/(U f) const {
    return (mat3(*this) /= f);
}

template<typename T>
template<typename U>
mat3<T> mat3<T>::operator+(U f) const {
    return (mat3(*this) += f);
}

template<typename T>
template<typename U>
mat3<T> mat3<T>::operator-(U f) const {
    return (mat3(*this) -= f);
}

template<typename T>
template<typename U>
mat3<T>& mat3<T>::operator*=(U f) {
    for (auto& val : _vec) {
        val *= f;
    }
    return *this;
}

template<typename T>
template<typename U>
mat3<T>& mat3<T>::operator/=(U f) {
    for (auto& val : _vec) {
        val /= f;
    }
    return *this;
}

template<typename T>
template<typename U>
mat3<T>& mat3<T>::operator+=(U f) {
    for (auto& val : _vec) {
        val += f;
    }
    return *this;
}

template<typename T>
template<typename U>
mat3<T>& mat3<T>::operator-=(U f) {
    for (auto& val : _vec) {
        val -= f;
    }
    return *this;
}

template<typename T>
bool mat3<T>::operator==(const mat3 &B) const {
    for (U8 i = 0; i < 9; ++i) {
        if (!COMPARE(mat[i], B.mat[i])) {
            return false;
        }
    }

    return true;
}

template<typename T>
template<typename U>
bool mat3<T>::operator==(const mat3<U> &B) const {
    for (U8 i = 0; i < 9; ++i) {
        if (!COMPARE(mat[i], B.mat[i])) {
            return false;
        }
    }

    return true;
}

template<typename T>
template<typename U>
bool mat3<T>::operator!=(const mat3<U> &B) const {
    for (U8 i = 0; i < 9; ++i) {
        if (!COMPARE(mat[i], B.mat[i])) {
            return true;
        }
    }

    return false;
}

template<typename T>
bool mat3<T>::operator!=(const mat3 &B) const {
    for (U8 i = 0; i < 9; ++i) {
        if (!COMPARE(mat[i], B.mat[i])) {
            return true;
        }
    }

    return false;
}

template<typename T>
bool mat3<T>::compare(const mat3 &B, F32 epsilon) const {
    for (U8 i = 0; i < 9; ++i) {
        if (!COMPARE_TOLERANCE(mat[i], B.mat[i], epsilon)) {
            return false;
        }
    }

    return true;
}

template<typename T>
template<typename U>
bool mat3<T>::compare(const mat3<U> &B, F32 epsilon) const {
    for (U8 i = 0; i < 9; ++i) {
        if (!COMPARE_TOLERANCE(mat[i], B.mat[i], epsilon)) {
            return false;
        }
    }

    return true;
}

template<typename T>
mat3<T>::operator T *() {
    return mat;
}

template<typename T>
mat3<T>::operator const T *() const {
    return mat;
}

template<typename T>
T& mat3<T>::operator[](I32 i) {
    return mat[i];
}

template<typename T>
const T mat3<T>::operator[](I32 i) const noexcept {
    return mat[i];
}

template<typename T>
T& mat3<T>::element(I8 row, I8 column) {
    return m[row][column];
}

template<typename T>
const T& mat3<T>::element(I8 row, I8 column) const {
    return m[row][column];
}

template<typename T>
template<typename U>
void mat3<T>::set(U m0, U m1, U m2, U m3, U m4, U m5, U m6, U m7, U m8) {
    mat[0] = static_cast<T>(m0);  mat[1] = static_cast<T>(m1); mat[3] = static_cast<T>(m3);
    mat[2] = static_cast<T>(m2);  mat[4] = static_cast<T>(m4); mat[5] = static_cast<T>(m5);
    mat[6] = static_cast<T>(m6);  mat[7] = static_cast<T>(m7); mat[8] = static_cast<T>(m8);
}

template<typename T>
template<typename U>
void mat3<T>::set(const U *matrix) {
    if (sizeof(T) == sizeof(U)) {
        memcpy(mat, matrix, sizeof(U) * 9);
    } else {
        set(matrix[0], matrix[1], matrix[2],
            matrix[3], matrix[4], matrix[5],
            matrix[6], matrix[7], matrix[8]);
    }
}

template<typename T>
template<typename U>
void mat3<T>::set(const mat2<U> &matrix) {
    const U zero = static_cast<U>(0);
    set(matrix[0], matrix[1], zero,
        matrix[2], matrix[3], zero,
        zero,      zero,      zero); //maybe mat[8] should be 1?
}

template<typename T>
template<typename U>
void mat3<T>::set(const mat3<U> &matrix) {
    set(matrix.mat);
}

template<typename T>
template<typename U>
void mat3<T>::set(const mat4<U> &matrix) {
    set(matrix[0], matrix[1], matrix[2],
        matrix[4], matrix[5], matrix[6],
        matrix[8], matrix[9], matrix[10]);
}

template<typename T>
template<typename U>
void mat3<T>::setRow(I32 index, const U value) {
    _vec[index].set(value);
}

template<typename T>
template<typename U>
void mat3<T>::setRow(I32 index, const vec3<U> &value) {
    _vec[index].set(value);
}

template<typename T>
template<typename U>
void mat3<T>::setRow(I32 index, const U x, const U y, const U z) {
    _vec[index].set(x, y, z);
}

template<typename T>
const vec3<T>& mat3<T>::getRow(I32 index) const {
    return _vec[index];
}

template<typename T>
template<typename U>
void mat3<T>::setCol(I32 index, const vec3<U> &value) {
    m[0][index] = static_cast<T>(value.x);
    m[1][index] = static_cast<T>(value.y);
    m[2][index] = static_cast<T>(value.z);
}

template<typename T>
template<typename U>
void mat3<T>::setCol(I32 index, const U value) {
    m[0][index] = static_cast<T>(value);
    m[1][index] = static_cast<T>(value);
    m[2][index] = static_cast<T>(value);
}

template<typename T>
template<typename U>
void mat3<T>::setCol(I32 index, const U x, const U y, const U z) {
    m[0][index] = static_cast<T>(x);
    m[1][index] = static_cast<T>(y);
    m[2][index] = static_cast<T>(z);
}

template<typename T>
vec3<T> mat3<T>::getCol(I32 index) const {
    return vec3<T>(m[0][index], m[1][index], m[2][index]);
}

template<typename T>
void mat3<T>::zero() {
    memset(mat, 0, 9 * sizeof(T));
}

template<typename T>
void mat3<T>::identity() {
    const T zero = static_cast<T>(0);
    const T one = static_cast<T>(1);
    mat[0] = one;  mat[1] = zero; mat[2] = zero;
    mat[3] = zero; mat[4] = one;  mat[5] = zero;
    mat[6] = zero; mat[7] = zero; mat[8] = one;
}

template<typename T>
bool mat3<T>::isIdentity() const {
    return (COMPARE(mat[0], 1) && IS_ZERO(mat[1])    && IS_ZERO(mat[2]) &&
            IS_ZERO(mat[3])    && COMPARE(mat[4], 1) && IS_ZERO(mat[5]) &&
            IS_ZERO(mat[6])    && IS_ZERO(mat[7])    && COMPARE(mat[8], 1));
}

template<typename T>
void mat3<T>::swap(mat3 &B) {
    std::swap(m[0][0], B.m[0][0]);
    std::swap(m[0][1], B.m[0][1]);
    std::swap(m[0][2], B.m[0][2]);

    std::swap(m[1][0], B.m[1][0]);
    std::swap(m[1][1], B.m[1][1]);
    std::swap(m[1][2], B.m[1][2]);

    std::swap(m[2][0], B.m[2][0]);
    std::swap(m[2][1], B.m[2][1]);
    std::swap(m[2][2], B.m[2][2]);
}

template<typename T>
T mat3<T>::det() const {
    return ((mat[0] * mat[4] * mat[8]) +
            (mat[3] * mat[7] * mat[2]) +
            (mat[6] * mat[1] * mat[5]) -
            (mat[6] * mat[4] * mat[2]) -
            (mat[3] * mat[1] * mat[8]) -
            (mat[0] * mat[7] * mat[5]));
}

template<typename T>
T mat3<T>::elementSum() const {
    return mat[0] + mat[1] + mat[2] +
           mat[3] + mat[4] + mat[5] +
           mat[6] + mat[7] + mat[8];
}

template<typename T>
void mat3<T>::inverse() {
    F32 idet = det();
    assert(!IS_ZERO(idet));
    idet = 1 / idet;

    set( (mat[4] * mat[8] - mat[7] * mat[5]) * idet,
        -(mat[1] * mat[8] - mat[7] * mat[2]) * idet,
         (mat[1] * mat[5] - mat[4] * mat[2]) * idet,
        -(mat[3] * mat[8] - mat[6] * mat[5]) * idet,
         (mat[0] * mat[8] - mat[6] * mat[2]) * idet,
        -(mat[0] * mat[5] - mat[3] * mat[2]) * idet,
         (mat[3] * mat[7] - mat[6] * mat[4]) * idet,
        -(mat[0] * mat[7] - mat[6] * mat[1]) * idet,
         (mat[0] * mat[4] - mat[3] * mat[1]) * idet);
}

template<typename T>
void mat3<T>::transpose() {
    set(mat[0], mat[3], mat[6],
        mat[1], mat[4], mat[7],
        mat[2], mat[5], mat[8]);
}

template<typename T>
void mat3<T>::inverseTranspose() {
    inverse();
    transpose();
}

template<typename T>
mat3<T> mat3<T>::getInverse() const {
    mat3<T> ret(mat);
    ret.inverse();
    return ret;
}

template<typename T>
void mat3<T>::getInverse(mat3<T> &ret) const {
    ret.set(mat);
    ret.inverse();
}

template<typename T>
mat3<T> mat3<T>::getTranspose() const {
    mat3<T> ret(mat);
    ret.transpose();
    return ret;
}

template<typename T>
void mat3<T>::getTranspose(mat3<T> &ret) const {
    ret.set(mat);
    ret.transpose();
}

template<typename T>
mat3<T> mat3<T>::getInverseTranspose() const {
    mat3<T> ret(getInverse());
    ret.transpose();
    return ret;
}

template<typename T>
void mat3<T>::getInverseTranspose(mat3<T> &ret) const {
    ret.set(this);
    ret.inverseTranspose();
}

template<typename T>
template<typename U>
void mat3<T>::fromRotation(const vec3<U> &v, Angle::RADIANS<U> angle) {
    fromRotation(v.x, v.y, v.z, angle);
}

template<typename T>
template<typename U>
void mat3<T>::fromRotation(U x, U y, U z, Angle::RADIANS<U> angle) {
    U c = std::cos(angle);
    U s = std::sin(angle);
    U l = static_cast<U>(Divide::Sqrt(static_cast<D64>(x * x + y * y + z * z)));

    l = l < EPSILON_F32 ? 1 : 1 / l;
    x *= l;
    y *= l;
    z *= l;

    U xy = x * y;
    U yz = y * z;
    U zx = z * x;
    U xs = x * s;
    U ys = y * s;
    U zs = z * s;
    U c1 = 1 - c;

    set(c1 * x * x + c, c1 * xy + zs,   c1 * zx - ys,
        c1 * xy - zs,   c1 * y * y + c, c1 * yz + xs;
        c1 * zx + ys,   c1 * yz - xs;   c1 * z * z + c);
}

template<typename T>
template<typename U>
void mat3<T>::fromXRotation(Angle::RADIANS<U> angle) {
    const U zero = static_cast<U>(0);
    const U one = static_cast<U>(1);
    U c = std::cos(angle);
    U s = std::sin(angle);

    set(one, zero, zero,
        zero, c,   s,
        zero, -s,  c);
}

template<typename T>
template<typename U>
void mat3<T>::fromYRotation(Angle::RADIANS<U> angle) {
    const U zero = static_cast<U>(0);
    const U one = static_cast<U>(1);

    U c = std::cos(angle);
    U s = std::sin(angle);

    set(c,    zero, -s,
        zero, one,   zero,
        s,    zero,  c);
}

template<typename T>
template<typename U>
void mat3<T>::fromZRotation(Angle::RADIANS<U> angle) {
    const U zero = static_cast<U>(0);
    const U one = static_cast<U>(1);

    U c = std::cos(angle);
    U s = std::sin(angle);

    set( c,    s,    zero,
        -s,    c,    zero,
         zero, zero, one);
}

// setScale replaces the main diagonal!
template<typename T>
template<typename U>
void mat3<T>::setScale(U x, U y, U z) {
    mat[0] = static_cast<T>(x);
    mat[4] = static_cast<T>(y);
    mat[8] = static_cast<T>(z);
}

template<typename T>
template<typename U>
void mat3<T>::setScale(const vec3<U> &v) {
    setScale(v.x, v.y, v.z);
}

template<typename T>
void mat3<T>::orthoNormalize(void) {
    vec3<T> x(mat[0], mat[1], mat[2]);
    x.normalize();
    vec3<T> y(mat[3], mat[4], mat[5]);
    vec3<T> z(cross(x, y));
    z.normalize();
    y.cross(z, x);
    y.normalize();

    set(x.x, x.y, x.z,
        y.x, y.y, y.z,
        z.x, z.y, z.z);
}

/***************
* mat4
***************/

template<typename T>
mat4<T>::mat4() noexcept
    : mat4({ 1, 0, 0, 0,
             0, 1, 0, 0,
             0, 0, 1, 0,
             0, 0, 0, 1 })
{
}

template<typename T>
template<typename U>
mat4<T>::mat4(U value) noexcept
    : mat{ static_cast<T>(value), static_cast<T>(value), static_cast<T>(value), static_cast<T>(value),
           static_cast<T>(value), static_cast<T>(value), static_cast<T>(value), static_cast<T>(value),
           static_cast<T>(value), static_cast<T>(value), static_cast<T>(value), static_cast<T>(value),
           static_cast<T>(value), static_cast<T>(value), static_cast<T>(value), static_cast<T>(value)}
{
}

template<typename T>
template<typename U>
mat4<T>::mat4(std::initializer_list<U> matrix) noexcept
    : mat4(matrix.begin())
{
}

template<typename T>
template<typename U>
mat4<T>::mat4(const U *values) noexcept
    : mat{ static_cast<T>(values[0]),  static_cast<T>(values[1]),  static_cast<T>(values[2]),  static_cast<T>(values[3]),
           static_cast<T>(values[4]),  static_cast<T>(values[5]),  static_cast<T>(values[6]),  static_cast<T>(values[7]),
           static_cast<T>(values[8]),  static_cast<T>(values[9]),  static_cast<T>(values[10]), static_cast<T>(values[11]),
           static_cast<T>(values[12]), static_cast<T>(values[13]), static_cast<T>(values[14]), static_cast<T>(values[15]) }
{
}

template<typename T>
template<typename U>
mat4<T>::mat4(const mat2<U> &B, bool zeroFill) noexcept
    : mat4({ B[0],              B[1],              static_cast<U>(0), static_cast<U>(0),
             B[2],              B[3],              static_cast<U>(0), static_cast<U>(0),
             static_cast<U>(0), static_cast<U>(0), static_cast<U>(zeroFill ? 0 : 1), static_cast<U>(0),
             static_cast<U>(0), static_cast<U>(0), static_cast<U>(0), static_cast<U>(zeroFill ? 0 : 1) })
{
}

template<typename T>
template<typename U>
mat4<T>::mat4(const mat3<U> &B, bool zeroFill) noexcept
    : mat4({ B[0],              B[1],              B[2],              static_cast<U>(0),
             B[3],              B[4],              B[5],              static_cast<U>(0),
             B[6],              B[7],              B[8],              static_cast<U>(0),
             static_cast<U>(0), static_cast<U>(0), static_cast<U>(0), static_cast<U>(zeroFill ? 0 : 1) })
{
}

template<typename T>
mat4<T>::mat4(const mat4 &B) noexcept
    : mat4(B.mat)
{
}

template<typename T>
template<typename U>
mat4<T>::mat4(const mat4<U> &B) noexcept
    : mat4(B.mat)
{
}

template<typename T>
template<typename U>
mat4<T>::mat4(const vec3<U> &translation, const vec3<U> &scale) noexcept
    : mat4({ scale.x,           static_cast<U>(0), static_cast<U>(0), static_cast<U>(0),
             static_cast<U>(0), scale.y,           static_cast<U>(0), static_cast<U>(0),
             static_cast<U>(0), static_cast<U>(0), scale.z,           static_cast<U>(0),
             translation.x,     translation.y,     translation.z,     static_cast<U>(1) })
{
}

template<typename T>
template<typename U>
mat4<T>::mat4(const vec3<U> &translation, const vec3<U> &scale, const mat3<U>& rotation) noexcept
    : mat4({scale.x,           static_cast<U>(0), static_cast<U>(0), static_cast<U>(0),
            static_cast<U>(0), scale.y,           static_cast<U>(0), static_cast<U>(0),
            static_cast<U>(0), static_cast<U>(0), scale.z,           static_cast<U>(0),
            static_cast<U>(0), static_cast<U>(0), static_cast<U>(0), static_cast<U>(1)})
{
    set(*this * mat4<U>(rotation, false));
    setTranslation(translation);
}

template<typename T>
template<typename U>
mat4<T>::mat4(const vec3<U> &translation) noexcept
    : mat4(translation.x, translation.y, translation.z)
{
}

template<typename T>
template<typename U>
mat4<T>::mat4(U translationX, U translationY, U translationZ) noexcept
    : mat4({static_cast<U>(0), static_cast<U>(0), static_cast<U>(0), static_cast<U>(0),
            static_cast<U>(0), static_cast<U>(0), static_cast<U>(0), static_cast<U>(0),
            static_cast<U>(0), static_cast<U>(0), static_cast<U>(0), static_cast<U>(0),
            translationX,      translationY,      translationZ,      static_cast<U>(1)})
{
}

template<typename T>
template<typename U>
mat4<T>::mat4(const vec3<U> &axis, Angle::RADIANS<U> angle) noexcept
    : mat4(axis.x, axis.y, axis.z, angle)
{
}

template<typename T>
template<typename U>
mat4<T>::mat4(U x, U y, U z, Angle::RADIANS<U> angle) noexcept
    : mat4()
{
    fromRotation(x, y, z, angle);
}

template<typename T>
template<typename U>
mat4<T>::mat4(const vec3<U> &eye, const vec3<U> &target, const vec3<U> &up) noexcept
    : mat4()
{
    lookAt(eye, target, up);
}

template<typename T>
template<typename U>
mat4<T>::mat4(const Rect<U> &orthoRect, const vec2<U> &clip) noexcept
    : mat4()
{
    ortho(orthoRect.left, orthoRect.right, orthoRect.bottom, orthoRect.top, clip.x, clip.y);
}

template<typename T>
template<typename U>
mat4<T>::mat4(const Plane<U>& reflectionPlane) noexcept
    : mat4()
{
    reflect(reflectionPlane);
}

template<typename T>
mat4<T>& mat4<T>::operator=(const mat4& other) noexcept {
    set(other);
    return *this;
}

template<typename T>
template<typename U>
mat4<T>& mat4<T>::operator=(const mat4<U>& other) noexcept {
    set(other);
    return *this;
}

template<typename T>
template<typename U>
vec2<U> mat4<T>::operator*(const vec2<U> &v) const {
    return *this * vec4<U>(v);
}

template<typename T>
template<typename U>
vec3<U> mat4<T>::operator*(const vec3<U> &v) const {
    return *this * vec4<U>(v);
}

template<typename T>
template<typename U>
vec4<U> mat4<T>::operator*(const vec4<U> &v) const {
    return vec4<U>(mat[0] * v.x + mat[4] * v.y + mat[8]  * v.z + mat[12] * v.w,
                   mat[1] * v.x + mat[5] * v.y + mat[9]  * v.z + mat[13] * v.w,
                   mat[2] * v.x + mat[6] * v.y + mat[10] * v.z + mat[14] * v.w,
                   mat[3] * v.x + mat[7] * v.y + mat[11] * v.z + mat[15] * v.w);
}

template<typename T>
template<typename U>
mat4<T> mat4<T>::operator*(const mat4<U>& matrix) const {
    mat4<T> retValue;
    Multiply(*this, matrix, retValue);
    return retValue;
}

template<typename T>
template<typename U>
mat4<T> mat4<T>::operator/(const mat4<U>& matrix) const {
    mat4<T> retValue;
    Multiply(*this, matrix.getInverse(), retValue);
    return retValue;
}

template<typename T>
template<typename U>
mat4<T> mat4<T>::operator+(const mat4<U> &matrix) const {
    return (mat4(*this) += matrix);
}

template<typename T>
template<typename U>
mat4<T> mat4<T>::operator-(const mat4<U> &matrix) const {
    return (mat4(*this) -= matrix);
}

template<typename T>
template<typename U>
mat4<T>& mat4<T>::operator*=(const mat4<U> &matrix) {
    Multiply(*this, matrix, *this);
    return *this;
}

template<typename T>
template<typename U>
mat4<T>& mat4<T>::operator/=(const mat4<U> &matrix) {
    Multiply(*this, matrix.getInverse(), *this);
    return *this;
}

template<typename T>
template<typename U>
mat4<T>& mat4<T>::operator+=(const mat4<U> &matrix) {
    for (U8 i = 0; i < 4; ++i) {
        _vec[i] += matrix._vec[i];
    }
    return *this;
}

template<typename T>
template<typename U>
mat4<T>& mat4<T>::operator-=(const mat4<U> &matrix) {
    for (U8 i = 0; i < 4; ++i) {
        _vec[i] -= matrix._vec[i];
    }

    return *this;
}

template<typename T>
template<typename U>
mat4<T> mat4<T>::operator*(U f) const {
    return (mat4(*this) *= f);
}

template<typename T>
template<typename U>
mat4<T> mat4<T>::operator/(U f) const {
    return (mat4(*this) /= f);
}

template<typename T>
template<typename U>
mat4<T> mat4<T>::operator+(U f) const {
    return (mat4(*this) += f);
}

template<typename T>
template<typename U>
mat4<T> mat4<T>::operator-(U f) const {
    return (mat4(*this) -= f);
}

template<typename T>
template<typename U>
mat4<T>& mat4<T>::operator*=(U f) {
    for (U8 i = 0; i < 4; ++i) {
        _vec[i] *= f;
    }

    return *this;
}

template<typename T>
template<typename U>
mat4<T>& mat4<T>::operator/=(U f) {
    for (U8 i = 0; i < 4; ++i) {
        _vec[i] /= f;
    }

    return *this;
}

template<typename T>
template<typename U>
mat4<T>& mat4<T>::operator+=(U f) {
    for (U8 i = 0; i < 4; ++i) {
        _vec[i] += f;
    }

    return *this;
}

template<typename T>
template<typename U>
mat4<T>& mat4<T>::operator-=(U f) {
    for (U8 i = 0; i < 4; ++i) {
        _vec[i] -= f;
    }

    return *this;
}

template<typename T>
bool mat4<T>::operator==(const mat4& B) const {
    for (U8 i = 0; i < 4; ++i) {
        if (_reg[i] != B._reg[i]) {
            return false;
        }
    }

    return true;
}

template<typename T>
bool mat4<T>::operator!=(const mat4 &B) const {
    for (U8 i = 0; i < 4; ++i) {
        if (_reg[i] != B._reg[i]) {
            return true;
        }
    }

    return false;
}

template<typename T>
template<typename U>
bool mat4<T>::operator==(const mat4<U>& B) const {
    /*
    // Add a small epsilon value to avoid 0.0 != 0.0
    if (!COMPARE(elementSum() + EPSILON_F32,
        B.elementSum() + EPSILON_F32)) {
        return false;
    }
    */
    for (U8 i = 0; i < 4; ++i) {
        for (U8 j = 0; j < 4; ++j) {
            if (!COMPARE(m[i][j], B.m[i][j])) {
                return false;
            }
        }
    }
    return true;
}

template<typename T>
template<typename U>
bool mat4<T>::operator!=(const mat4<U> &B) const {
    /*
    // Add a small epsilon value to avoid 0.0 != 0.0
    if (!COMPARE(elementSum() + EPSILON_F32,
        B.elementSum() + EPSILON_F32)) {
        return true;
    }
    */
    for (U8 i = 0; i < 4; ++i) {
        for (U8 j = 0; j < 4; ++j) {
            if (!COMPARE(m[i][j], B.m[i][j])) {
                return true;
            }
        }
    }

    return false;
}

template<typename T>
bool mat4<T>::compare(const mat4 &B, F32 epsilon) const {
    for (U8 i = 0; i < 16; ++i) {
        if (!COMPARE_TOLERANCE(mat[i], B.mat[i], epsilon)) {
            return false;
        }
    }

    return true;
}

template<typename T>
template<typename U>
bool mat4<T>::compare(const mat4<U> &B, F32 epsilon) const {
    for (U8 i = 0; i < 16; ++i) {
        if (!COMPARE_TOLERANCE(mat[i], B.mat[i], epsilon)) {
            return false;
        }
    }

    return true;
}

template<typename T>
mat4<T>::operator T *() {
    return mat;
}

template<typename T>
mat4<T>::operator const T *() const {
    return mat;
}

template<typename T>
T& mat4<T>::operator[](I32 i) noexcept {
    return mat[i];
}

template<typename T>
const T& mat4<T>::operator[](I32 i) const noexcept {
    return mat[i];
}

template<typename T>
T& mat4<T>::element(I8 row, I8 column) {
    return m[row][column];
}

template<typename T>
const T& mat4<T>::element(I8 row, I8 column) const {
    return m[row][column];
}

template<typename T>
template<typename U>
void mat4<T>::set(std::initializer_list<U> matrix) {
    set(matrix.begin());
}

template<typename T>
template<typename U>
void mat4<T>::set(U const *matrix) {
    if (sizeof(T) == sizeof(U)) {
        memcpy(mat, matrix, sizeof(U) * 16);
    } else {
        for (U8 i = 0; i < 16; ++i) {
            mat[i] = static_cast<T>(matrix[i]);
        }
    }
}

template<typename T>
template<typename U>
void mat4<T>::set(const mat2<U> &matrix) {
    memset(mat, 0, 16 * sizeof(T));

    mat[0] = matrix[0];  mat[1] = matrix[1];
    mat[4] = matrix[2];  mat[5] = matrix[3];
}

template<typename T>
template<typename U>
void mat4<T>::set(const mat3<U> &matrix) {
    memset(mat, 0, 16 * sizeof(T));

    mat[0] = matrix[0]; mat[1] = matrix[1]; mat[2]  = matrix[2];
    mat[4] = matrix[3]; mat[5] = matrix[4]; mat[6]  = matrix[5];
    mat[8] = matrix[6]; mat[9] = matrix[7]; mat[10] = matrix[8];
}

template<typename T>
template<typename U>
void mat4<T>::set(const mat4<U> &matrix) {
    set(matrix.mat);
}

template<typename T>
template<typename U>
inline void mat4<T>::set(const vec3<U> &translation, const vec3<U> &scale, const mat4<U>& rotation) {
    set(scale.x,           static_cast<U>(0), static_cast<U>(0), static_cast<U>(0),
        static_cast<U>(0), scale.y,           static_cast<U>(0), static_cast<U>(0),
        static_cast<U>(0), static_cast<U>(0), scale.z,           static_cast<U>(0),
        static_cast<U>(0), static_cast<U>(0), static_cast<U>(0), static_cast<U>(1));
    set(*this * rotation);
    setTranslation(translation);
}

template<typename T>
template<typename U>
void mat4<T>::setRow(I32 index, const U value) {
    _vec[index].set(value);
}

template<typename T>
template<typename U>
void mat4<T>::setRow(I32 index, const vec4<U> &value) {
    _vec[index].set(value);
}

template<typename T>
template<typename U>
void mat4<T>::setRow(I32 index, const U x, const U y, const U z, const U w) {
    _vec[index].set(x, y, z, w);
}

template<typename T>
const vec4<T>& mat4<T>::getRow(I32 index) const {
    return _vec[index];
}

template<typename T>
template<typename U>
void mat4<T>::setCol(I32 index, const vec4<U> &value) {
    setCol(index, value.x, value.y, value.z, value.w);
}

template<typename T>
template<typename U>
void mat4<T>::setCol(I32 index, const U value) {
    m[0][index] = static_cast<T>(value);
    m[1][index] = static_cast<T>(value);
    m[2][index] = static_cast<T>(value);
    m[3][index] = static_cast<T>(value);
}

template<typename T>
template<typename U>
void mat4<T>::setCol(I32 index, const U x, const U y, const U z, const U w) {
    m[0][index] = static_cast<T>(x);
    m[1][index] = static_cast<T>(y);
    m[2][index] = static_cast<T>(z);
    m[3][index] = static_cast<T>(w);
}

template<typename T>
vec4<T> mat4<T>::getCol(I32 index) const {
    return vec4<T>(m[0][index], m[1][index], m[2][index], m[3][index]);
}

template<typename T>
void mat4<T>::zero() {
    memset(mat, 0, sizeof(T) * 16);
}

template<typename T>
void mat4<T>::identity() {
    memset(mat, 0, 16 * sizeof(T));
    m[0][0] = m[1][1] = m[2][2] = m[3][3] = static_cast<T>(1);
}

template<typename T>
bool mat4<T>::isIdentity() const {
    return (COMPARE(mat[0], 1) && IS_ZERO(mat[1])    && IS_ZERO(mat[2])     && IS_ZERO(mat[3])
            IS_ZERO(mat[4])    && COMPARE(mat[5], 1) && IS_ZERO(mat[6])     && IS_ZERO(mat[7])
            IS_ZERO(mat[8])    && IS_ZERO(mat[9])    && COMPARE(mat[10], 1) && IS_ZERO(mat[11]),
            IS_ZERO(mat[12])   && IS_ZERO(mat[13])   && IS_ZERO(mat[14])    && COMPARE(mat[15], 1));
}

template<typename T>
void mat4<T>::swap(mat4 &B) {
    std::swap(m[0][0], B.m[0][0]);
    std::swap(m[0][1], B.m[0][1]);
    std::swap(m[0][2], B.m[0][2]);
    std::swap(m[0][3], B.m[0][3]);

    std::swap(m[1][0], B.m[1][0]);
    std::swap(m[1][1], B.m[1][1]);
    std::swap(m[1][2], B.m[1][2]);
    std::swap(m[1][3], B.m[1][3]);

    std::swap(m[2][0], B.m[2][0]);
    std::swap(m[2][1], B.m[2][1]);
    std::swap(m[2][2], B.m[2][2]);
    std::swap(m[2][3], B.m[2][3]);

    std::swap(m[3][0], B.m[3][0]);
    std::swap(m[3][1], B.m[3][1]);
    std::swap(m[3][2], B.m[3][2]);
    std::swap(m[3][3], B.m[3][3]);
}

template<typename T>
T mat4<T>::det() const noexcept {
    return ((mat[0] * mat[5] * mat[10]) + (mat[4] * mat[9] * mat[2]) +
            (mat[8] * mat[1] * mat[6])  - (mat[8] * mat[5] * mat[2]) -
            (mat[4] * mat[1] * mat[10]) - (mat[0] * mat[9] * mat[6]));
}

template<typename T>
T mat4<T>::elementSum() const {
    return mat[0]  + mat[1]  + mat[2]  + mat[3]  +
           mat[4]  + mat[5]  + mat[6]  + mat[7]  +
           mat[8]  + mat[9]  + mat[10] + mat[11] +
           mat[12] + mat[13] + mat[14] + mat[15];
}

template<typename T>
void mat4<T>::orthoNormalize() {
    _comp.right.normalize();
    _comp.up.normalize();
    _comp.dir.normalize();
}

template<typename T>
void mat4<T>::inverse() {
    Inverse(mat, mat);
}

template<>
FORCE_INLINE void mat4<F32>::inverse() {
    *this = GetInverse(*this);
}

template<typename T>
void mat4<T>::transpose() {
    set({ mat[0], mat[4], mat[8],  mat[12],
          mat[1], mat[5], mat[9],  mat[13],
          mat[2], mat[6], mat[10], mat[14],
          mat[3], mat[7], mat[11], mat[15] });
}

template<typename T>
void mat4<T>::inverseTranspose() {
    inverse();
    transpose();
}

template<typename T>
mat4<T> mat4<T>::transposeRotation() const {
    set({ mat[0],   mat[4],  mat[8], mat[3],
          mat[1],   mat[5],  mat[9], mat[7],
          mat[2],   mat[6], mat[10], mat[11],
          mat[12], mat[13], mat[14], mat[15] });
}

template<typename T>
mat4<T> mat4<T>::getInverse() const {
    mat4 ret;
    Inverse(mat, ret.mat);
    return ret;
}

template<>
FORCE_INLINE mat4<F32> mat4<F32>::getInverse() const {
    mat4<F32> ret;
    GetInverse(*this, ret);
    return ret;
}

template<typename T>
void mat4<T>::getInverse(mat4 &ret) const {
    Inverse(mat, ret.mat);
}

template<>
FORCE_INLINE void mat4<F32>::getInverse(mat4<F32> &ret) const {
    GetInverse(*this, ret);
}

template<typename T>
mat4<T> mat4<T>::getTranspose() const {
    return mat4({mat[0], mat[4], mat[8],  mat[12],
                 mat[1], mat[5], mat[9],  mat[13],
                 mat[2], mat[6], mat[10], mat[14],
                 mat[3], mat[7], mat[11], mat[15]});
}

template<typename T>
void mat4<T>::getTranspose(mat4 &out) const {
    out.set({mat[0], mat[4], mat[8],  mat[12],
             mat[1], mat[5], mat[9],  mat[13],
             mat[2], mat[6], mat[10], mat[14],
             mat[3], mat[7], mat[11], mat[15]});
}

template<typename T>
mat4<T> mat4<T>::getInverseTranspose() const {
    mat4 ret;
    Inverse(mat, ret.mat);
    ret.transpose();
    return ret;
}

template<>
FORCE_INLINE mat4<F32> mat4<F32>::getInverseTranspose() const {
    mat4<F32> ret;
    GetInverse(*this, ret);
    ret.transpose();
    return ret;
}

template<typename T>
void mat4<T>::getInverseTranspose(mat4 &ret) const {
    Inverse(mat, ret.mat);
    ret.transpose();
}

template<>
FORCE_INLINE void mat4<F32>::getInverseTranspose(mat4<F32> &ret) const {
    GetInverse(*this, ret);
    ret.transpose();
}

template<typename T>
mat4<T> mat4<T>::getTransposeRotation() const {
    return mat4(mat[0],  mat[4],  mat[8],  mat[3],
                mat[1],  mat[5],  mat[9],  mat[7],
                mat[2],  mat[6],  mat[10], mat[11],
                mat[12], mat[13], mat[14], mat[15]);
}

template<typename T>
void mat4<T>::getTransposeRotation(mat4 &ret) const {
    ret.set(mat[0],  mat[4],  mat[8],  mat[3],
            mat[1],  mat[5],  mat[9],  mat[7],
            mat[2],  mat[6],  mat[10], mat[11],
            mat[12], mat[13], mat[14], mat[15]);
}

template<typename T>
template<typename U>
void mat4<T>::fromRotation(U x, U y, U z, Angle::RADIANS<U> angle) {
    vec3<U> v(x, y, z);
    v.normalize();

    U c = std::cos(angle);
    U s = std::sin(angle);

    U xx = v.x * v.x;
    U yy = v.y * v.y;
    U zz = v.z * v.z;
    U xy = v.x * v.y;
    U yz = v.y * v.z;
    U zx = v.z * v.x;
    U xs = v.x * s;
    U ys = v.y * s;
    U zs = v.z * s;

    set((1 - c) * xx + c,        (1 - c) * xy + zs,       (1 - c) * zx - ys,       static_cast<U>(mat[3]),
        (1 - c) * xy - zs,       (1 - c) * yy + c,        (1 - c) * yz + xs,       static_cast<U>(mat[7]),
        (1 - c) * zx + ys,       (1 - c) * yz - xs,       (1 - c) * zz + c,        static_cast<U>(mat[11]),
        static_cast<U>(mat[12]), static_cast<U>(mat[13]), static_cast<U>(mat[14]), static_cast<U>(mat[15]))
}

template<typename T>
template<typename U>
void mat4<T>::fromXRotation(Angle::RADIANS<U> angle) {
    U c = std::cos(angle);
    U s = std::sin(angle);

    mat[5] = static_cast<T>(c);
    mat[9] = static_cast<T>(-s);
    mat[6] = static_cast<T>(s);
    mat[10] = static_cast<T>(c);
}

template<typename T>
template<typename U>
void mat4<T>::fromYRotation(Angle::RADIANS<U> angle) {
    U c = std::cos(angle);
    U s = std::sin(angle);

    mat[0] = static_cast<T>(c);
    mat[8] = static_cast<T>(s);
    mat[2] = static_cast<T>(-s);
    mat[10] = static_cast<T>(c);
}

template<typename T>
template<typename U>
void mat4<T>::fromZRotation(Angle::RADIANS<U> angle) {
    U c = std::cos(angle);
    U s = std::sin(angle);

    mat[0] = static_cast<T>(c);
    mat[4] = static_cast<T>(-s);
    mat[1] = static_cast<T>(s);
    mat[5] = static_cast<T>(c);
}

template<typename T>
template<typename U>
void mat4<T>::setTranslation(const vec3<U> &v) {
    setTranslation(v.x, v.y, v.z);
}

template<typename T>
template<typename U>
void mat4<T>::setTranslation(U x, U y, U z) noexcept {
    mat[12] = static_cast<T>(x);
    mat[13] = static_cast<T>(y);
    mat[14] = static_cast<T>(z);
}

template<typename T>
template<typename U>
void mat4<T>::setScale(U x, U y, U z) noexcept {
    mat[0]  = static_cast<T>(x);
    mat[5]  = static_cast<T>(y);
    mat[10] = static_cast<T>(z);
}

template<typename T>
template<typename U>
void mat4<T>::setScale(const vec3<U> &v) {
    setScale(v.x, v.y, v.z);
}

template<typename T>
template<typename U>
vec3<U> mat4<T>::transform(const vec3<U> &v, bool homogeneous) const {
    return  homogeneous ? transformHomogeneous(v)
                        : transformNonHomogeneous(v);
}

template<typename T>
template<typename U>
vec3<U> mat4<T>::transformHomogeneous(const vec3<U> &v) const {
    //Transforms the given 3-D vector by the matrix, projecting the result back into <i>w</i> = 1. (OGRE reference)
    F32 fInvW = 1.0f / (m[0][3] * v.x + m[1][3] * v.y +
                        m[2][3] * v.z + m[3][3]);

    return vec3<U>((m[0][0] * v.x + m[1][1] * v.y + m[2][0] * v.z + m[3][0]) * fInvW,
                   (m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z + m[3][1]) * fInvW,
                   (m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z + m[3][2]) * fInvW);
}

template<typename T>
template<typename U>
vec3<U> mat4<T>::transformNonHomogeneous(const vec3<U> &v) const {
    return *this * vec4<U>(v, static_cast<U>(0));
}

template<typename T>
template<typename U>
void mat4<T>::translate(const vec3<U> &v) {
    translate(v.x, v.y, v.z);
}

template<typename T>
template<typename U>
void mat4<T>::translate(U x, U y, U z) {
    mat[12] += static_cast<T>(x);
    mat[13] += static_cast<T>(y);
    mat[14] += static_cast<T>(z);
}

template<typename T>
template<typename U>
void mat4<T>::scale(const vec3<U> &v) {
    scale(v.x, v.y, v.z);
}

template<typename T>
template<typename U>
void mat4<T>::scale(U x, U y, U z) {
    mat[0]  *= static_cast<T>(x);
    mat[4]  *= static_cast<T>(y);
    mat[8]  *= static_cast<T>(z);
    mat[1]  *= static_cast<T>(x);
    mat[5]  *= static_cast<T>(y);
    mat[9]  *= static_cast<T>(z);
    mat[2]  *= static_cast<T>(x);
    mat[6]  *= static_cast<T>(y);
    mat[10] *= static_cast<T>(z);
    mat[3]  *= static_cast<T>(x);
    mat[7]  *= static_cast<T>(y);
    mat[11] *= static_cast<T>(z);
}

template<typename T>
template<typename U>
vec3<U> mat4<T>::getTranslation() const {
    return vec3<U>(mat[12], mat[13], mat[14]);
}

template<typename T>
mat4<T> mat4<T>::getRotation() const {

    const T zero = static_cast<T>(0);
    const T one = static_cast<T>(1);
    mat4 ret ({ mat[0],  mat[1], mat[2],  zero,
                mat[4],  mat[5], mat[6],  zero,
                mat[8],  mat[9], mat[10], zero,
                zero,    zero,   zero,    one });
    ret.orthoNormalize();
    return ret;
}

template<typename T>
template<typename U>
const mat4<T>& mat4<T>::reflect(U x, U y, U z, U w) {
    return reflect(Plane<U>(x, y, z, w));
}

template<typename T>
template<typename U>
const mat4<T>& mat4<T>::reflect(const Plane<U> &plane) {
    const U zero = static_cast<U>(0);
    const U one = static_cast<U>(1);

    const vec4<U> &eq = plane.getEquation();

    U x = eq.x;
    U y = eq.y;
    U z = eq.z;
    U d = eq.w;

    *this = mat4({ -2 * x * x + 1,  -2 * y * x,      -2 * z * x,      zero,
                   -2 * x * y,      -2 * y * y + 1,  -2 * z * y,      zero,
                   -2 * x * z,      -2 * y * z,      -2 * z * z + 1,  zero,
                   -2 * x * d,      -2 * y * d,      -2 * z * d,      one }) * *this;

    return *this;
}

template<typename T>
template<typename U>
void mat4<T>::lookAt(const vec3<U> &eye, const vec3<U> &target, const vec3<U> &up) {
    vec3<U> zAxis(eye - target);
    zAxis.normalize();
    vec3<U> xAxis(Cross(up, zAxis));
    xAxis.normalize();
    vec3<U> yAxis(Cross(zAxis, xAxis));
    yAxis.normalize();

    m[0][0] = static_cast<T>(xAxis.x);
    m[1][0] = static_cast<T>(xAxis.y);
    m[2][0] = static_cast<T>(xAxis.z);
    m[3][0] = static_cast<T>(-xAxis.dot(eye));

    m[0][1] = static_cast<T>(yAxis.x);
    m[1][1] = static_cast<T>(yAxis.y);
    m[2][1] = static_cast<T>(yAxis.z);
    m[3][1] = static_cast<T>(-yAxis.dot(eye));

    m[0][2] = static_cast<T>(zAxis.x);
    m[1][2] = static_cast<T>(zAxis.y);
    m[2][2] = static_cast<T>(zAxis.z);
    m[3][2] = static_cast<T>(-zAxis.dot(eye));

    m[0][3] = static_cast<T>(0);
    m[1][3] = static_cast<T>(0);
    m[2][3] = static_cast<T>(0);
    m[3][3] = static_cast<T>(1);
}

template<typename T>
template<typename U>
void mat4<T>::ortho(U left, U right, U bottom, U top, U zNear, U zFar) {
    zero();

    m[0][0] =  static_cast<T>(2.0f / (right - left));
    m[1][1] =  static_cast<T>(2.0f / (top - bottom));
    m[2][2] = -static_cast<T>(2.0f / (zFar - zNear));
    m[3][3] =  static_cast<T>(1);


    m[3][0] = -static_cast<T>(to_F32(right + left) / (right - left));
    m[3][1] = -static_cast<T>(to_F32(top + bottom) / (top - bottom));
    m[3][2] = -static_cast<T>(to_F32(zFar + zNear) / (zFar - zNear));
}

template<typename T>
template<typename U>
void mat4<T>::perspective(Angle::DEGREES<U> fovyRad, U aspect, U zNear, U zFar) {
    assert(!IS_ZERO(aspect));
    assert(zFar > zNear);

    Angle::RADIANS<F32> tanHalfFovy = std::tan(Angle::to_RADIANS(fovyRad) * 0.5f);

    zero();

    m[0][0] =  static_cast<T>(1.0f / (aspect * tanHalfFovy));
    m[1][1] =  static_cast<T>(1.0f / (tanHalfFovy));
    m[2][2] = -static_cast<T>(to_F32(zFar + zNear) / (zFar - zNear));
    m[2][3] = -static_cast<T>(1);
    m[3][2] = -static_cast<T>((2.0f * zFar * zNear) / (zFar - zNear));
}

template<typename T>
template<typename U>
void mat4<T>::frustum(U left, U right, U bottom, U top, U nearVal, U farVal) {
    zero();

    m[0][0] = static_cast<T>((2.0f * nearVal) / (right - left));
    m[1][1] = static_cast<T>((2.0f * nearVal) / (top - bottom));
    m[2][0] = static_cast<T>(to_F32(right + left) / (right - left));
    m[2][1] = static_cast<T>(to_F32(top + bottom) / (top - bottom));
    m[2][2] = -static_cast<T>(to_F32(farVal + nearVal) / (farVal - nearVal));
    m[2][3] = static_cast<T>(-1);
    m[3][2] = -static_cast<T>((2.0f * farVal * nearVal) / (farVal - nearVal));
}

template<typename T>
template<typename U>
void mat4<T>::extractMat3(mat3<U> &matrix3) const {
    matrix3.m[0][0] = static_cast<U>(m[0][0]);
    matrix3.m[0][1] = static_cast<U>(m[0][1]);
    matrix3.m[0][2] = static_cast<U>(m[0][2]);
    matrix3.m[1][0] = static_cast<U>(m[1][0]);
    matrix3.m[1][1] = static_cast<U>(m[1][1]);
    matrix3.m[1][2] = static_cast<U>(m[1][2]);
    matrix3.m[2][0] = static_cast<U>(m[2][0]);
    matrix3.m[2][1] = static_cast<U>(m[2][1]);
    matrix3.m[2][2] = static_cast<U>(m[2][2]);
}

template<typename T>
FORCE_INLINE void mat4<T>::Multiply(const mat4<T>& matrixA, const mat4<T>& matrixB, mat4<T>& ret) {
    for (U8 i = 0; i < 4; ++i) {
        const vec4<T>& rowA = matrixA.getRow(i);
        ret.setRow(i, matrixB.getRow(0) * rowA[0] + matrixB.getRow(1) * rowA[1] + matrixB.getRow(2) * rowA[2] + matrixB.getRow(3) * rowA[3]);
    }
}

template<>
FORCE_INLINE void mat4<F32>::Multiply(const mat4<F32>& matrixA, const mat4<F32>& matrixB, mat4<F32>& ret) {
    M4x4_SSE(matrixA, matrixB, ret);
}

// Copyright 2011 The Closure Library Authors. All Rights Reserved.
template<typename T>
FORCE_INLINE void mat4<T>::Inverse(const T* in, T* out) {
    T m00 = in[0], m10 = in[1], m20 = in[2], m30 = in[3];
    T m01 = in[4], m11 = in[5], m21 = in[6], m31 = in[7];
    T m02 = in[8], m12 = in[9], m22 = in[10], m32 = in[11];
    T m03 = in[12], m13 = in[13], m23 = in[14], m33 = in[15];

    T a0 = m00 * m11 - m10 * m01;
    T a1 = m00 * m21 - m20 * m01;
    T a2 = m00 * m31 - m30 * m01;
    T a3 = m10 * m21 - m20 * m11;
    T a4 = m10 * m31 - m30 * m11;
    T a5 = m20 * m31 - m30 * m21;
    T b0 = m02 * m13 - m12 * m03;
    T b1 = m02 * m23 - m22 * m03;
    T b2 = m02 * m33 - m32 * m03;
    T b3 = m12 * m23 - m22 * m13;
    T b4 = m12 * m33 - m32 * m13;
    T b5 = m22 * m33 - m32 * m23;

    // should be accurate enough
    F32 idet = to_F32(a0) * b5 - a1 * b4 + a2 * b3 + a3 * b2 - a4 * b1 + a5 * b0;

    if (!IS_ZERO(idet)) {
        idet = 1.0f / idet;

        out[0] = static_cast<T>((m11 * b5 - m21 * b4 + m31 * b3) * idet);
        out[1] = static_cast<T>((-m10 * b5 + m20 * b4 - m30 * b3) * idet);
        out[2] = static_cast<T>((m13 * a5 - m23 * a4 + m33 * a3) * idet);
        out[3] = static_cast<T>((-m12 * a5 + m22 * a4 - m32 * a3) * idet);
        out[4] = static_cast<T>((-m01 * b5 + m21 * b2 - m31 * b1) * idet);
        out[5] = static_cast<T>((m00 * b5 - m20 * b2 + m30 * b1) * idet);
        out[6] = static_cast<T>((-m03 * a5 + m23 * a2 - m33 * a1) * idet);
        out[7] = static_cast<T>((m02 * a5 - m22 * a2 + m32 * a1) * idet);
        out[8] = static_cast<T>((m01 * b4 - m11 * b2 + m31 * b0) * idet);
        out[9] = static_cast<T>((-m00 * b4 + m10 * b2 - m30 * b0) * idet);
        out[10] = static_cast<T>((m03 * a4 - m13 * a2 + m33 * a0) * idet);
        out[11] = static_cast<T>((-m02 * a4 + m12 * a2 - m32 * a0) * idet);
        out[12] = static_cast<T>((-m01 * b3 + m11 * b1 - m21 * b0) * idet);
        out[13] = static_cast<T>((m00 * b3 - m10 * b1 + m20 * b0) * idet);
        out[14] = static_cast<T>((-m03 * a3 + m13 * a1 - m23 * a0) * idet);
        out[15] = static_cast<T>((m02 * a3 - m12 * a1 + m22 * a0) * idet);
    } else {
        memcpy(out, in, sizeof(T) * 16);
    }
}

}; //namespace Divide
#endif //_MATH_MATRICES_INL_
