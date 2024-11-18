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
#ifndef DVD_PLATFORM_DATA_TYPES_H_
#define DVD_PLATFORM_DATA_TYPES_H_

#include <type_traits> //std::underlying_type_t
#include <cstddef> //std::byte
#include <limits>

namespace Divide
{

    // "Exact" number of bits
    using U8 = uint8_t;
    using U16 = uint16_t;
    using U32 = uint32_t;
    using U64 = uint64_t;
    using I8 = int8_t;
    using I16 = int16_t;
    using I32 = int32_t;
    using I64 = int64_t;

    using u8 = U8;
    using u16 = U16;
    using u32 = U32;
    using u64 = U64;
    using s8 = I8;
    using s16 = I16;
    using s32 = I32;
    using s64 = I64;

    // "At least" number of bits
    using U8x = uint_least8_t;
    using U16x = uint_least16_t;
    using U32x = uint_least32_t;
    using U64x = uint_least64_t;
    using I8x = int_least8_t;
    using I16x = int_least16_t;
    using I32x = int_least32_t;
    using I64x = int_least64_t;

    using u8x = U8x;
    using u16x = U16x;
    using u32x = U32x;
    using u64x = U64x;
    using s8x = I8x;
    using s16x = I16x;
    using s32x = I32x;
    using s64x = I64x;

    //double is 8 bytes with Microsoft's compiler)
    using F32  = float;
    using D64  = double;
    using D128 = long double;

    using r32 = F32;
    using r64 = D64;
    using r128 = D128;

    // Just a name to use as a reminder that these values shoul be be in the 0.0f to 1.0f range
    using F32_NORM = F32;
    // Just a name to use as a reminder that these values shoul be be in the -1.0f to 1.0f range
    using F32_SNORM = F32;

    using bufferPtr = void*;

    using Byte = std::byte;

constexpr Byte Byte_ZERO = Byte{0u};

constexpr U8  U8_MAX  = std::numeric_limits<U8>::max();
constexpr U16 U16_MAX = std::numeric_limits<U16>::max();
constexpr U32 U24_MAX = (1 << 24u) - 1u;
constexpr U32 U32_MAX = std::numeric_limits<U32>::max();
constexpr U64 U64_MAX = std::numeric_limits<U64>::max();
constexpr I8  I8_MAX  = std::numeric_limits<I8>::max();
constexpr I16 I16_MAX = std::numeric_limits<I16>::max();
constexpr I32 I24_MAX = (1 << 23) - 1;
constexpr I32 I32_MAX = std::numeric_limits<I32>::max();
constexpr I64 I64_MAX = std::numeric_limits<I64>::max();
constexpr F32 F32_MAX = std::numeric_limits<F32>::max();
constexpr D64 D64_MAX = std::numeric_limits<D64>::max();

constexpr U8x  U8x_MAX  = std::numeric_limits<U8x>::max();
constexpr U16x U16x_MAX = std::numeric_limits<U16x>::max();
constexpr U32x U32x_MAX = std::numeric_limits<U32x>::max();
constexpr U64x U64x_MAX = std::numeric_limits<U64x>::max();
constexpr I8x  I8x_MAX  = std::numeric_limits<I8x>::max();
constexpr I16x I16x_MAX = std::numeric_limits<I16x>::max();
constexpr I32x I32x_MAX = std::numeric_limits<I32x>::max();
constexpr I64x I64x_MAX = std::numeric_limits<I64x>::max();

constexpr u8  u8_MAX  = U8_MAX;
constexpr u16 u16_MAX = U16_MAX;
constexpr u32 u32_MAX = U32_MAX;
constexpr u64 u64_MAX = U64_MAX;
constexpr s8  s8_MAX  = I8_MAX;
constexpr s16 s16_MAX = I16_MAX;
constexpr s32 s32_MAX = I32_MAX;
constexpr s64 s64_MAX = I64_MAX;

constexpr u8x  u8x_MAX  = U8x_MAX;
constexpr u16x u16x_MAX =U16x_MAX;
constexpr u32x u32x_MAX =U32x_MAX;
constexpr u64x u64x_MAX =U64x_MAX;
constexpr s8x s8x_MAX  = I8x_MAX;
constexpr s16x s16x_MAX =I16x_MAX;
constexpr s32x s32x_MAX =I32x_MAX;
constexpr s64x s64x_MAX =I64x_MAX;

constexpr F32 F32_INFINITY = std::numeric_limits<F32>::infinity();

constexpr F32 F32_LOWEST = std::numeric_limits<F32>::lowest();
constexpr I64 I64_LOWEST = std::numeric_limits<I64>::lowest();

constexpr U8  U8_ONE  = U8(1u);
constexpr U16 U16_ONE = U16(1u);
constexpr U32 U32_ONE = 1u;
constexpr U64 U64_ONE = 1u;
constexpr I8  I8_ONE  = I8(1);
constexpr I16 I16_ONE = I16(1);
constexpr I32 I32_ONE = 1;
constexpr I64 I64_ONE = 1;
constexpr F32 F32_ONE = 1.f;
constexpr D64 D64_ONE = 1.0;

constexpr U8  U8_ZERO  = U8(0u);
constexpr U16 U16_ZERO = U16(0u);
constexpr U32 U32_ZERO = 0u;
constexpr U64 U64_ZERO = 0u;
constexpr I8  I8_ZERO  = I8(0);
constexpr I16 I16_ZERO = I16(0);
constexpr I32 I32_ZERO = 0;
constexpr I64 I64_ZERO = 0;
constexpr F32 F32_ZERO = 0.f;
constexpr D64 D64_ZERO = 0.0;

//ref: https://foonathan.net/2020/09/move-forward/
// static_cast to rvalue reference
#define MOV(...) static_cast<std::remove_reference_t<decltype(__VA_ARGS__)>&&>(__VA_ARGS__)

// static_cast to identity
// The extra && aren't necessary as discussed above, but make it more robust in case it's used with a non-reference.
#define FWD(...) static_cast<decltype(__VA_ARGS__)&&>(__VA_ARGS__)

    union P32
    {
        U32 i = 0u;
        U8  b[4];

        P32() = default;
        P32( const U32 val ) noexcept ;
        P32( const U8 b1, const U8 b2, const U8 b3, const U8 b4 ) noexcept;
        P32( U8* bytes ) noexcept;
    };

    static const P32 P32_FLAGS_TRUE = { 1u, 1u, 1u, 1u };
    static const P32 P32_FLAGS_FALSE = { 0u, 0u, 0u, 0u };

    inline bool operator==( const P32& lhs, const P32& rhs ) noexcept
    {
        return lhs.i == rhs.i;
    }
    inline bool operator!=( const P32& lhs, const P32& rhs ) noexcept
    {
        return lhs.i != rhs.i;
    }

    namespace detail
    {
        void internal_assert(const bool condition);
    } //namespace detail

    template <typename From, typename To>
    struct static_caster
    {
        To operator()( From p ) noexcept
        {
            return static_cast<To>(p);
        }
    };

    /*
    template<typename Enum>
    constexpr U32 operator"" _u32 ( Enum value )
    {
    return static_cast<U32>(value);
    }*/

    template<typename Type>
    using BaseType = std::underlying_type_t<Type>;

    template <typename Type> requires std::is_integral_v<Type>
    constexpr auto to_base( const Type value ) -> Type
    {
        return value;
    }

    template<typename T> requires std::is_floating_point_v<T>
    constexpr bool isNonNegative(const T value)
    {
        return !std::signbit(value);
    }

    template <typename Type> requires std::is_enum_v<Type>
    constexpr auto to_base( const Type value ) -> BaseType<Type>
    {
        return static_cast<BaseType<Type>>(value);
    }

    template <typename T>
    constexpr size_t to_size( const T value )
    {
        if constexpr ( std::is_floating_point_v<T> )
        {
            detail::internal_assert(isNonNegative(value));
        }

        return static_cast<size_t>(value);
    }

    template <typename T>
    constexpr U64 to_U64( const T value )
    {
        if constexpr ( std::is_floating_point_v<T> )
        {
            detail::internal_assert(isNonNegative(value));
        }

        return static_cast<U64>(value);
    }

    template <typename T>
    constexpr U32 to_U32( const T value )
    {
        if constexpr ( std::is_floating_point_v<T> )
        {
            detail::internal_assert(isNonNegative(value));
        }

        return static_cast<U32>(value);
    }

    template <typename T>
    constexpr U16 to_U16( const T value )
    {
        if constexpr ( std::is_floating_point_v<T> )
        {
            detail::internal_assert(isNonNegative(value));
        }

        return static_cast<U16>(value);
    }

    template<typename T>
    constexpr U8 to_U8( const T value )
    {
        if constexpr ( std::is_floating_point_v<T> )
        {
            detail::internal_assert(isNonNegative(value));
        }

        return static_cast<U8>(value);
    }

    template <typename T>
    constexpr I64 to_I64( const T value )
    {
        return static_cast<I64>(value);
    }

    template <typename T>
    constexpr I32 to_I32( const T value )
    {
        return static_cast<I32>(value);
    }

    template <typename T>
    constexpr I16 to_I16( const T value )
    {
        return static_cast<I16>(value);
    }

    template <typename T>
    constexpr I8 to_I8( const T value )
    {
        return static_cast<I8>(value);
    }
    template <typename T>
    constexpr F32 to_F32( const T value )
    {
        return static_cast<F32>(value);
    }

    template <typename T>
    constexpr D64 to_D64( const T value )
    {
        return static_cast<D64>(value);
    }

    template<typename T>
    constexpr D128 to_D128( const T value )
    {
        return static_cast<D128>(value);
    }

    template<typename T>
    constexpr Byte to_byte( const T value )
    {
        if constexpr ( std::is_floating_point_v<T> )
        {
            detail::internal_assert(isNonNegative(value));
        }

        return static_cast<Byte>(value);
    }

    //Ref: https://stackoverflow.com/questions/7416699/how-to-define-24bit-data-type-in-c
    constexpr I32 INT24_MAX = 8388607;
    constexpr U32 UINT24_MAX = to_U32(INT24_MAX * 2);

    //ref: http://codereview.stackexchange.com/questions/51235/udp-network-server-client-for-gaming-using-boost-asio
    struct counter
    {
        size_t _count;
        
        counter& operator=( const size_t val ) noexcept;
        explicit counter( const size_t count = 0 ) noexcept;
        operator size_t() const noexcept;
        counter& operator++() noexcept;
        counter operator++( int ) noexcept;

        bool operator==( const counter& ) const = default;
    };


    // Type promotion
    // ref: https://janmr.com/blog/2010/08/cpp-templates-usual-arithmetic-conversions/
    template <bool C, typename T, typename F> struct choose_type             { using type = F; };
    template <typename T, typename F>         struct choose_type<true, T, F> { using type = T; };

    template <bool C, typename T, typename F> using choose_type_t  = choose_type<C,T,F>::type;

    template <typename T> struct promote       { using type = T;   };
    template <>           struct promote<bool> { using type = I32; };
    template <>           struct promote<I16>  { using type = I32; };
    template <>           struct promote<U16>  { using type = choose_type_t<sizeof( I16 )  <  sizeof( I32 ), I32, U32>; };
    template <>           struct promote<I8>   { using type = choose_type_t<sizeof( char ) <= sizeof( I32 ), I32, U32>; };
    template <>           struct promote<U8>   { using type = choose_type_t<sizeof( char ) <  sizeof( I32 ), I32, U32>; };

    template <>           struct promote<char> : public promote<choose_type_t<std::numeric_limits<char>::is_signed, signed char, unsigned char>> {};
    template <>           struct promote<wchar_t>
    {
        using type = choose_type_t<std::numeric_limits<wchar_t>::is_signed,
                                   choose_type_t<sizeof( wchar_t ) <= sizeof( I32 ), I32, long>,
                                   choose_type_t<sizeof( wchar_t ) <= sizeof( I32 ), U32, unsigned long>>;
    };

    template <typename T> using promote_t = promote<T>::type;

    template <typename T, bool override = false> struct type_rank;

    template <> struct type_rank<U8>                                      { static constexpr I32 rank =  1; };
    template <> struct type_rank<I8>                                      { static constexpr I32 rank =  2; };
    template <> struct type_rank<I16>                                     { static constexpr I32 rank =  3; };
    template <> struct type_rank<U16>                                     { static constexpr I32 rank =  4; };
    template <> struct type_rank<I32>                                     { static constexpr I32 rank =  5; };
    template <> struct type_rank<U32>                                     { static constexpr I32 rank =  6; };
    template <> struct type_rank<long>                                    { static constexpr I32 rank =  7; };
    template <> struct type_rank<unsigned long>                           { static constexpr I32 rank =  8; };
    template <> struct type_rank<I64, std::is_same_v<I64, long>>          { static constexpr I32 rank =  9; };
    template <> struct type_rank<U64, std::is_same_v<U64, unsigned long>> { static constexpr I32 rank = 10; };
    template <> struct type_rank<F32>                                     { static constexpr I32 rank = 11; };
    template <> struct type_rank<D64>                                     { static constexpr I32 rank = 12; };
    template <> struct type_rank<D128>                                    { static constexpr I32 rank = 13; };

    template <typename A, typename B>
    struct resolve_uac2
    {
        using return_type = choose_type_t<type_rank<A>::rank >= type_rank<B>::rank, A, B>;
    };

    template<typename A, typename B>
    struct resolve_uac : public resolve_uac2<promote_t<A>, promote_t<B>>
    {
    };

    template<>
    struct resolve_uac2<long, U32>
    {
        using return_type = choose_type_t<sizeof( long ) == sizeof( U32 ), unsigned long, long>;
    };

    template<>
    struct resolve_uac2<U32, long> : public resolve_uac2<long, U32>
    {
    };

    template<typename A, typename B> constexpr typename resolve_uac<A, B>::return_type add(      const A& a, const B& b ) noexcept { return a + b; }
    template<typename A, typename B> constexpr typename resolve_uac<A, B>::return_type subtract( const A& a, const B& b ) noexcept { return a - b; }
    template<typename A, typename B> constexpr typename resolve_uac<A, B>::return_type divide(   const A& a, const B& b ) noexcept { return a / b; }
    template<typename A, typename B> constexpr typename resolve_uac<A, B>::return_type multiply( const A& a, const B& b ) noexcept { return a * b; }

    template <typename ToCheck, std::size_t ExpectedSize, std::size_t RealSize = sizeof( ToCheck )>
    constexpr void check_size()
    {
        static_assert(ExpectedSize == RealSize, "Wrong data size!");
    }

    template < template <typename...> class base, typename derived>
    struct is_base_of_template_impl
    {
        template<typename... Ts>
        static constexpr std::true_type  test(const base<Ts...>*);
        static constexpr std::false_type test(...);
        using type = decltype(test(std::declval<derived*>()));
    };

    template < template <typename...> class base, typename derived>
    using is_base_of_template = typename is_base_of_template_impl<base, derived>::type;

    //ref: https://stackoverflow.com/questions/17793298/c-class-wrapper-around-fundamental-types
    template<typename T, typename tagType>
    struct primitiveWrapper
    {
        using value_type = T;

        value_type value{};

        primitiveWrapper() :value() {}

        template<typename U, typename allowed = std::enable_if_t<std::is_constructible_v<T, U>>>
        primitiveWrapper(const U v) :value(v) {}

        template<typename U>
        explicit primitiveWrapper(const primitiveWrapper<U, tagType>& rhs) : value(static_cast<U>(rhs)) {}

        //modifiers
        template<typename U> FORCE_INLINE primitiveWrapper& operator= ( const U v) { value   = v; return *this; }
        template<typename U> FORCE_INLINE primitiveWrapper& operator+=( const U v) { value  += v; return *this; }
        template<typename U> FORCE_INLINE primitiveWrapper& operator-=( const U v) { value  -= v; return *this; }
        template<typename U> FORCE_INLINE primitiveWrapper& operator*=( const U v) { value  *= v; return *this; }
        template<typename U> FORCE_INLINE primitiveWrapper& operator/=( const U v) { value  /= v; return *this; }
        template<typename U> FORCE_INLINE primitiveWrapper& operator%=( const U v) { value  %= v; return *this; }
        template<typename U> FORCE_INLINE primitiveWrapper& operator&=( const U v) { value  &= v; return *this; }
        template<typename U> FORCE_INLINE primitiveWrapper& operator|=( const U v) { value  |= v; return *this; }
        template<typename U> FORCE_INLINE primitiveWrapper& operator^=( const U v) { value  ^= v; return *this; }
        template<typename U> FORCE_INLINE primitiveWrapper& operator<<=(const U v) { value <<= v; return *this; }
        template<typename U> FORCE_INLINE primitiveWrapper& operator>>=(const U v) { value >>= v; return *this; }

        FORCE_INLINE primitiveWrapper& operator++()    { ++value; return *this; }
        FORCE_INLINE primitiveWrapper& operator--()    { --value; return *this; }
        FORCE_INLINE primitiveWrapper  operator++(int) { return primitiveWrapper(value++); }
        FORCE_INLINE primitiveWrapper  operator--(int) { return primitiveWrapper(value--); }

        //accessors
        FORCE_INLINE T*               operator&  ()       { return &value;}
        FORCE_INLINE                  operator T&()       { return value; }
        FORCE_INLINE                  operator T () const { return value; }
        FORCE_INLINE primitiveWrapper operator+  () const { return primitiveWrapper(+value); }
        FORCE_INLINE primitiveWrapper operator-  () const { return primitiveWrapper(-value); }
        FORCE_INLINE primitiveWrapper operator!  () const { return primitiveWrapper(!value); }
        FORCE_INLINE primitiveWrapper operator~  () const { return primitiveWrapper(~value); }

        //friends
        friend primitiveWrapper operator+ (primitiveWrapper pw, primitiveWrapper v ) { return pw  += v; }
        friend primitiveWrapper operator+ (primitiveWrapper pw, T                v ) { return pw  += v; }
        friend primitiveWrapper operator+ (T                v,  primitiveWrapper pw) { return primitiveWrapper(v) += pw; }
        friend primitiveWrapper operator- (primitiveWrapper pw, primitiveWrapper v ) { return pw  -= v; }
        friend primitiveWrapper operator- (primitiveWrapper pw,                T v ) { return pw  -= v; }
        friend primitiveWrapper operator- (T v,                 primitiveWrapper pw) { return primitiveWrapper(v) -= pw; }
        friend primitiveWrapper operator* (primitiveWrapper pw, primitiveWrapper v ) { return pw  *= v; }
        friend primitiveWrapper operator* (primitiveWrapper pw,                T v ) { return pw  *= v; }
        friend primitiveWrapper operator* (T v,                 primitiveWrapper pw) { return primitiveWrapper(v) *= pw; }
        friend primitiveWrapper operator/ (primitiveWrapper pw, primitiveWrapper v ) { return pw  /= v; }
        friend primitiveWrapper operator/ (primitiveWrapper pw,                T v ) { return pw  /= v; }
        friend primitiveWrapper operator/ (T v,                 primitiveWrapper pw) { return primitiveWrapper(v) /= pw; }
        friend primitiveWrapper operator% (primitiveWrapper pw, primitiveWrapper v ) { return pw  %= v; }
        friend primitiveWrapper operator% (primitiveWrapper pw,                T v ) { return pw  %= v; }
        friend primitiveWrapper operator% (T v,                 primitiveWrapper pw) { return primitiveWrapper(v) %= pw; }
        friend primitiveWrapper operator& (primitiveWrapper pw, primitiveWrapper v ) { return pw  &= v; }
        friend primitiveWrapper operator& (primitiveWrapper pw,                T v ) { return pw  &= v; }
        friend primitiveWrapper operator& (T v,                 primitiveWrapper pw) { return primitiveWrapper(v) &= pw; }
        friend primitiveWrapper operator| (primitiveWrapper pw, primitiveWrapper v ) { return pw  |= v; }
        friend primitiveWrapper operator| (primitiveWrapper pw,                T v ) { return pw  |= v; }
        friend primitiveWrapper operator| (T v,                 primitiveWrapper pw) { return primitiveWrapper(v) |= pw; }
        friend primitiveWrapper operator^ (primitiveWrapper pw, primitiveWrapper v ) { return pw  ^= v; }
        friend primitiveWrapper operator^ (primitiveWrapper pw,                T v ) { return pw  ^= v; }
        friend primitiveWrapper operator^ (T v,                 primitiveWrapper pw) { return primitiveWrapper(v) ^= pw; }
        friend primitiveWrapper operator<<(primitiveWrapper pw, primitiveWrapper v ) { return pw <<= v; }
        friend primitiveWrapper operator<<(primitiveWrapper pw,                T v ) { return pw <<= v; }
        friend primitiveWrapper operator<<(T v,                 primitiveWrapper pw) { return primitiveWrapper(v) <<= pw; }
        friend primitiveWrapper operator>>(primitiveWrapper pw, primitiveWrapper v ) { return pw >>= v; }
        friend primitiveWrapper operator>>(primitiveWrapper pw,                T v ) { return pw >>= v; }
        friend primitiveWrapper operator>>(T v,                 primitiveWrapper pw) { return primitiveWrapper(v) >>= pw; }

        template<typename U, typename R = std::common_type_t<T, U>> friend primitiveWrapper<R, tagType> operator+ (const primitiveWrapper pw, const primitiveWrapper<U, tagType> v) { return primitiveWrapper<R, tagType>(pw)  += v; }
        template<typename U, typename R = std::common_type_t<T, U>> friend primitiveWrapper<R, tagType> operator- (const primitiveWrapper pw, const primitiveWrapper<U, tagType> v) { return primitiveWrapper<R, tagType>(pw)  -= v; }
        template<typename U, typename R = std::common_type_t<T, U>> friend primitiveWrapper<R, tagType> operator* (const primitiveWrapper pw, const primitiveWrapper<U, tagType> v) { return primitiveWrapper<R, tagType>(pw)  *= v; }
        template<typename U, typename R = std::common_type_t<T, U>> friend primitiveWrapper<R, tagType> operator/ (const primitiveWrapper pw, const primitiveWrapper<U, tagType> v) { return primitiveWrapper<R, tagType>(pw)  /= v; }
        template<typename U, typename R = std::common_type_t<T, U>> friend primitiveWrapper<R, tagType> operator% (const primitiveWrapper pw, const primitiveWrapper<U, tagType> v) { return primitiveWrapper<R, tagType>(pw)  %= v; }
        template<typename U, typename R = std::common_type_t<T, U>> friend primitiveWrapper<R, tagType> operator& (const primitiveWrapper pw, const primitiveWrapper<U, tagType> v) { return primitiveWrapper<R, tagType>(pw)  &= v; }
        template<typename U, typename R = std::common_type_t<T, U>> friend primitiveWrapper<R, tagType> operator| (const primitiveWrapper pw, const primitiveWrapper<U, tagType> v) { return primitiveWrapper<R, tagType>(pw)  |= v; }
        template<typename U, typename R = std::common_type_t<T, U>> friend primitiveWrapper<R, tagType> operator^ (const primitiveWrapper pw, const primitiveWrapper<U, tagType> v) { return primitiveWrapper<R, tagType>(pw)  ^= v; }
        template<typename U, typename R = std::common_type_t<T, U>> friend primitiveWrapper<R, tagType> operator<<(const primitiveWrapper pw, const primitiveWrapper<U, tagType> v) { return primitiveWrapper<R, tagType>(pw) <<= v; }
        template<typename U, typename R = std::common_type_t<T, U>> friend primitiveWrapper<R, tagType> operator>>(const primitiveWrapper pw, const primitiveWrapper<U, tagType> v) { return primitiveWrapper<R, tagType>(pw) >>= v; }
    };
#define DIVIDE_UNUSED(X) ((void)X)

#pragma region PROPERTY_SETTERS_GETTERS

#if !defined(EXP)
#define EXP( x ) x 
#define GET_3RD_ARG(arg1, arg2, arg3, ...) arg3
#define GET_4TH_ARG(arg1, arg2, arg3, arg4, ...) arg4
#define GET_5TH_ARG(arg1, arg2, arg3, arg4, arg5, ...) arg5
#define GET_6TH_ARG(arg1, arg2, arg3, arg4, arg5, arg6, ...) arg6
#endif //EXP

    template <typename T> struct is_const_char : std::false_type {};
    template <std::size_t N> struct is_const_char<const char[N]> : std::true_type {};
    template <typename T> concept concept_const_char = is_const_char<T>::value;

    //https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention?view=msvc-170
    //Any argument that doesn't fit in 8 bytes, or isn't 1, 2, 4, or 8 bytes, must be passed by reference. A single argument is never spread across multiple registers.
    template<typename T>
    constexpr bool fits_in_registers()
    {
        constexpr auto size = sizeof( T );
        return size == 1u || size == 2u || size == 4u || size == 8u;
    }

    template<typename T>
    constexpr bool can_be_returned_by_value()
    {
        return std::is_trivially_copyable_v<T> || std::is_copy_assignable_v<T>;
    }

    template<typename T>
    constexpr bool pass_by_value()
    {
        return fits_in_registers<T>() && can_be_returned_by_value<T>();
    }

template<typename Type>
using GET_RET_TYPE = std::conditional_t<pass_by_value<Type>(), Type, Type const&>;

template<typename Type>
using GET_PASS_TYPE = std::conditional_t<pass_by_value<Type>(), std::conditional_t<std::is_move_assignable_v<Type>, Type, const Type>, Type const&>;

#define PROPERTY_GET_SET(Type, Name)                                                         \
public:                                                                                      \
    inline void Name(GET_PASS_TYPE<Type> val) noexcept { _##Name = val; }        \
    [[nodiscard]] inline Type& Name() noexcept { return _##Name; }                     \
    [[nodiscard]] inline GET_RET_TYPE<Type> Name() const noexcept { return _##Name; }

#define PROPERTY_GET(Type, Name)                                                             \
public:                                                                                      \
    [[nodiscard]] inline GET_RET_TYPE<Type> Name() const noexcept { return _##Name; }

#define VIRTUAL_PROPERTY_GET_SET(Type, Name)                                             \
public:                                                                                  \
    virtual void Name(GET_PASS_TYPE<Type> val) noexcept { _##Name = val; }         \
    [[nodiscard]] virtual Type& Name() noexcept { return _##Name; }                      \
    [[nodiscard]] virtual GET_RET_TYPE<Type> Name() const noexcept { return _##Name; }

#define VIRTUAL_PROPERTY_GET(Type, Name)                                              \
public:                                                                               \
    [[nodiscard]] virtual GET_RET_TYPE<Type> Name() const noexcept { return _##Name; }

#define POINTER_GET_SET(Type, Name)                                                  \
public:                                                                              \
    inline void Name(Type* const val) noexcept { _##Name = val; }              \
    [[nodiscard]] inline Type* Name() const noexcept { return _##Name; }

#define POINTER_GET(Type, Name)                                                     \
public:                                                                             \
   [[nodiscard]] inline Type* Name() const noexcept { return _##Name; }

#define PROPERTY_GET_INTERNAL(Type, Name)                                                  \
protected:                                                                                 \
    [[nodiscard]] inline GET_RET_TYPE<Type> Name() const noexcept { return _##Name; }

#define VIRTUAL_PROPERTY_GET_INTERNAL(Type, Name)                                     \
protected:                                                                            \
    [[nodiscard]] virtual GET_RET_TYPE<Type> Name() const noexcept { return _##Name; }

#define POINTER_GET_INTERNAL(Type, Name)                                             \
protected:                                                                           \
    [[nodiscard]] inline Type* Name() noexcept { return _##Name; }

#define PROPERTY_SET_INTERNAL(Type, Name)                                             \
protected:                                                                            \
    inline void Name(GET_PASS_TYPE<Type> val) noexcept { _##Name = val; }

#define VIRTUAL_PROPERTY_SET_INTERNAL(Type, Name)                                \
protected:                                                                       \
    virtual void Name(GET_PASS_TYPE<Type> val) noexcept { _##Name = val; }

#define POINTER_SET_INTERNAL(Type, Name)                                \
protected:                                                              \
    inline void Name(Type* const val) noexcept { _##Name = val; }

#define PROPERTY_GET_SET_INTERNAL(Type, Name) \
protected: \
    PROPERTY_SET_INTERNAL(Type, Name) \
    PROPERTY_GET_INTERNAL(Type, Name)

#define VIRTUAL_PROPERTY_GET_SET_INTERNAL(Type, Name) \
protected: \
    VIRTUAL_PROPERTY_SET_INTERNAL(Type, Name) \
    VIRTUAL_PROPERTY_GET_INTERNAL(Type, Name)

#define POINTER_GET_SET_INTERNAL(Type, Name) \
protected: \
    POINTER_SET_INTERNAL(Type, Name) \
    POINTER_GET_INTERNAL(Type, Name)

#define PROPERTY_R_1_ARGS(Type)
#define PROPERTY_RW_1_ARGS(Type)
#define PROPERTY_R_IW_1_ARGS(Type)
#define VIRTUAL_PROPERTY_R_1_ARGS(Type)
#define VIRTUAL_PROPERTY_R_IW_1_ARGS(Type)
#define POINTER_R_1_ARGS(Type)
#define POINTER_RW_1_ARGS(Type)
#define POINTER_R_IW_1_ARGS(Type)
#define REFERENCE_R_1_ARGS(Type)
#define REFERENCE_RW_1_ARGS(Type)

#define PROPERTY_RW_1_ARGS_INTERNAL(Type)
#define VIRTUAL_PROPERTY_RW_1_ARGS(Type)
#define VIRTUAL_PROPERTY_RW_1_ARGS_INTERNAL(Type)
#define POINTER_RW_1_ARGS_INTERNAL(Type)
#define REFERENCE_RW_1_ARGS_INTERNAL(Type)

    //------------- PROPERTY_RW
#define PROPERTY_R_3_ARGS(Type, Name, Val) \
protected: \
    Type _##Name = Val; \
    PROPERTY_GET(Type, Name) \
public:\
    static_assert(true, "")

#define PROPERTY_R_IW_3_ARGS(Type, Name, Val) \
protected: \
    Type _##Name = Val; \
    PROPERTY_GET(Type, Name) \
    PROPERTY_SET_INTERNAL(Type, Name) \
public: \
    static_assert(true, "")

#define PROPERTY_RW_3_ARGS(Type, Name, Val) \
protected: \
    Type _##Name = Val; \
    PROPERTY_GET_SET(Type, Name) \
public: \
    static_assert(true, "")

#define PROPERTY_R_2_ARGS(Type, Name) \
protected: \
    Type _##Name; \
    PROPERTY_GET(Type, Name) \
public:\
    static_assert(true, "")

#define PROPERTY_R_IW_2_ARGS(Type, Name) \
protected: \
    Type _##Name; \
    PROPERTY_GET(Type, Name) \
    PROPERTY_SET_INTERNAL(Type, Name) \
public: \
    static_assert(true, "")

#define PROPERTY_RW_2_ARGS(Type, Name) \
protected: \
    Type _##Name; \
    PROPERTY_GET_SET(Type, Name) \
public: \
    static_assert(true, "")

//------------- PROPERTY_RW_INTERNAL
#define PROPERTY_RW_3_ARGS_INTERNAL(Type, Name, Val) \
protected: \
    Type _##Name = Val; \
    PROPERTY_GET_SET_INTERNAL(Type, Name) \
public: \
    static_assert(true, "")

#define PROPERTY_RW_2_ARGS_INTERNAL(Type, Name) \
protected: \
    Type _##Name; \
    PROPERTY_GET_SET_INTERNAL(Type, Name)\
public: \
    static_assert(true, "")

//------------------- VIRTUAL_PROPERTY_RW
#define VIRTUAL_PROPERTY_R_3_ARGS(Type, Name, Val) \
protected: \
    Type _##Name = Val; \
    VIRTUAL_PROPERTY_GET(Type, Name) \
public: \
    static_assert(true, "")

#define VIRTUAL_PROPERTY_R_IW_3_ARGS(Type, Name, Val) \
protected: \
    Type _##Name = Val; \
    VIRTUAL_PROPERTY_GET(Type, Name) \
    VIRTUAL_PROPERTY_SET_INTERNAL(Type, Name) \
public: \
    static_assert(true, "")

#define VIRTUAL_PROPERTY_RW_3_ARGS(Type, Name, Val) \
protected: \
    Type _##Name = Val; \
    VIRTUAL_PROPERTY_GET_SET(Type, Name) \
public: \
    static_assert(true, "")

#define VIRTUAL_PROPERTY_R_2_ARGS(Type, Name) \
protected: \
    Type _##Name; \
    VIRTUAL_PROPERTY_GET(Type, Name) \
public: \
    static_assert(true, "")

#define VIRTUAL_PROPERTY_R_IW_2_ARGS(Type, Name) \
protected: \
    Type _##Name; \
    VIRTUAL_PROPERTY_GET(Type, Name) \
    VIRTUAL_PROPERTY_SET_INTERNAL(Type, Name) \
public:\
    static_assert(true, "")

#define VIRTUAL_PROPERTY_RW_2_ARGS(Type, Name) \
protected: \
    Type _##Name; \
    VIRTUAL_PROPERTY_GET_SET(Type, Name) \
public: \
    static_assert(true, "")

//------------------- VIRTUAL_PROPERTY_RW_INTERNAL
#define VIRTUAL_PROPERTY_RW_3_ARGS_INTERNAL(Type, Name, Val) \
protected: \
    Type _##Name = Val; \
    VIRTUAL_PROPERTY_GET_SET_INTERNAL(Type, Name) \
public: \
    static_assert(true, "")

#define VIRTUAL_PROPERTY_RW_2_ARGS_INTERNAL(Type, Name) \
protected: \
    Type _##Name; \
    VIRTUAL_PROPERTY_GET_SET_INTERNAL(Type, Name) \
public: \
    static_assert(true, "")

//-------------------- POINTER_RW
#define POINTER_R_3_ARGS(Type, Name, Val) \
protected: \
    Type* _##Name = Val; \
    POINTER_GET(Type, Name) \
public: \
    static_assert(true, "")

#define POINTER_R_IW_3_ARGS(Type, Name, Val) \
protected: \
    Type* _##Name = Val; \
    POINTER_GET(Type, Name) \
    POINTER_SET_INTERNAL(Type, Name) \
public: \
    static_assert(true, "")

#define POINTER_RW_3_ARGS(Type, Name, Val) \
protected: \
    Type* _##Name = Val; \
    POINTER_GET_SET(Type, Name) \
public: \
    static_assert(true, "")

#define POINTER_R_2_ARGS(Type, Name) \
protected: \
    Type* _##Name{nullptr}; \
    POINTER_GET(Type, Name) \
public: \
    static_assert(true, "")

#define POINTER_R_IW_2_ARGS(Type, Name) \
protected: \
    Type* _##Name{nullptr}; \
    POINTER_GET(Type, Name) \
    POINTER_SET_INTERNAL(Type, Name) \
public: \
    static_assert(true, "")

#define POINTER_RW_2_ARGS(Type, Name) \
protected: \
    Type* _##Name{nullptr}; \
    POINTER_GET_SET(Type, Name) \
public: \
    static_assert(true, "")

//-------------------- POINTER_RW_INTERNAL
#define POINTER_RW_3_ARGS_INTERNAL(Type, Name, Val) \
protected: \
    Type* _##Name = Val; \
    POINTER_GET_SET_INTERNAL(Type, Name) \
public: \
    static_assert(true, "")

#define POINTER_RW_2_ARGS_INTERNAL(Type, Name) \
protected: \
    Type* _##Name{nullptr}; \
    POINTER_GET_SET_INTERNAL(Type, Name) \
public: \
    static_assert(true, "")

//-------------------- REFERENCE_RW
#define REFERENCE_R_2_ARGS(Type, Name) \
protected: \
    Type& _##Name; \
    PROPERTY_GET(Type, Name) \
public: \
    static_assert(true, "")

#define REFERENCE_RW_2_ARGS(Type, Name) \
protected: \
    Type& _##Name; \
    PROPERTY_GET_SET(Type, Name) \
public: \
    static_assert(true, "")

//-------------------- REFERENCE_RW_INTERNAL
#define REFERENCE_RW_2_ARGS_INTERNAL(Type, Name) \
protected: \
    Type& _##Name; \
    PROPERTY_GET_SET_INTERNAL(Type, Name) \
public: \
    static_assert(true, "")

#define ___DETAIL_PROPERTY_RW_INTERNAL(...) EXP(GET_4TH_ARG(__VA_ARGS__, PROPERTY_RW_3_ARGS_INTERNAL, PROPERTY_RW_2_ARGS_INTERNAL, PROPERTY_RW_1_ARGS_INTERNAL, ))
#define ___DETAIL_VIRTUAL_PROPERTY_RW_INTERNAL(...) EXP(GET_4TH_ARG(__VA_ARGS__, VIRTUAL_PROPERTY_RW_3_ARGS_INTERNAL, VIRTUAL_PROPERTY_RW_2_ARGS_INTERNAL, VIRTUAL_PROPERTY_RW_1_ARGS_INTERNAL, ))
#define ___DETAIL_POINTER_RW_INTERNAL(...) EXP(GET_4TH_ARG(__VA_ARGS__, POINTER_RW_3_ARGS_INTERNAL, POINTER_RW_2_ARGS_INTERNAL, POINTER_RW_1_ARGS_INTERNAL, ))
#define ___DETAIL_REFERENCE_RW_INTERNAL(...) EXP(GET_4TH_ARG(__VA_ARGS__, REFERENCE_RW_3_ARGS_INTERNAL, REFERENCE_RW_2_ARGS_INTERNAL, REFERENCE_RW_1_ARGS_INTERNAL, ))

#define ___DETAIL_PROPERTY_R(...) EXP(GET_4TH_ARG(__VA_ARGS__, PROPERTY_R_3_ARGS, PROPERTY_R_2_ARGS, PROPERTY_R_1_ARGS, ))
#define ___DETAIL_PROPERTY_RW(...) EXP(GET_4TH_ARG(__VA_ARGS__, PROPERTY_RW_3_ARGS, PROPERTY_RW_2_ARGS, PROPERTY_RW_1_ARGS, ))
#define ___DETAIL_PROPERTY_R_IW(...) EXP(GET_4TH_ARG(__VA_ARGS__, PROPERTY_R_IW_3_ARGS, PROPERTY_R_IW_2_ARGS, PROPERTY_R_IW_1_ARGS, ))

#define ___DETAIL_VIRTUAL_PROPERTY_R(...) EXP(GET_4TH_ARG(__VA_ARGS__, VIRTUAL_PROPERTY_R_3_ARGS, VIRTUAL_PROPERTY_R_2_ARGS, VIRTUAL_PROPERTY_R_1_ARGS, ))
#define ___DETAIL_VIRTUAL_PROPERTY_RW(...) EXP(GET_4TH_ARG(__VA_ARGS__, VIRTUAL_PROPERTY_RW_3_ARGS, VIRTUAL_PROPERTY_RW_2_ARGS, VIRTUAL_PROPERTY_RW_1_ARGS, ))
#define ___DETAIL_VIRTUAL_PROPERTY_R_IW(...) EXP(GET_4TH_ARG(__VA_ARGS__, VIRTUAL_PROPERTY_R_IW_3_ARGS, VIRTUAL_PROPERTY_R_IW_2_ARGS, VIRTUAL_PROPERTY_R_IW_1_ARGS, ))

#define ___DETAIL_POINTER_R(...) EXP(GET_4TH_ARG(__VA_ARGS__, POINTER_R_3_ARGS, POINTER_R_2_ARGS, POINTER_R_1_ARGS, ))
#define ___DETAIL_POINTER_RW(...) EXP(GET_4TH_ARG(__VA_ARGS__, POINTER_RW_3_ARGS, POINTER_RW_2_ARGS, POINTER_RW_1_ARGS, ))
#define ___DETAIL_POINTER_R_IW(...) EXP(GET_4TH_ARG(__VA_ARGS__, POINTER_R_IW_3_ARGS, POINTER_R_IW_2_ARGS, POINTER_R_IW_1_ARGS, ))

#define ___DETAIL_REFERENCE_R(...) EXP(GET_3RD_ARG(__VA_ARGS__, REFERENCE_R_2_ARGS, REFERENCE_R_1_ARGS, ))
#define ___DETAIL_REFERENCE_RW(...) EXP(GET_3RD_ARG(__VA_ARGS__, REFERENCE_RW_2_ARGS, REFERENCE_RW_1_ARGS, ))

/// Convenience method to add a class member with public read access but protected write access
#define PROPERTY_R(...) EXP(___DETAIL_PROPERTY_R(__VA_ARGS__)(__VA_ARGS__))

/// Convenience method to add a class member with public read access and write access
#define PROPERTY_RW(...) EXP(___DETAIL_PROPERTY_RW(__VA_ARGS__)(__VA_ARGS__))

/// Convenience method to add a class member with public read access but protected write access including a protected accessor
#define PROPERTY_R_IW(...) EXP(___DETAIL_PROPERTY_R_IW(__VA_ARGS__)(__VA_ARGS__))

/// Convenience method to add a class member with public read access but protected write access
#define VIRTUAL_PROPERTY_R(...) EXP(___DETAIL_VIRTUAL_PROPERTY_R(__VA_ARGS__)(__VA_ARGS__))

/// Convenience method to add a class member with public read access and write access
#define VIRTUAL_PROPERTY_RW(...) EXP(___DETAIL_VIRTUAL_PROPERTY_RW(__VA_ARGS__)(__VA_ARGS__))

/// Convenience method to add a class member with public read access but protected write access including a protected accessor
#define VIRTUAL_PROPERTY_R_IW(...) EXP(___DETAIL_VIRTUAL_PROPERTY_R_IW(__VA_ARGS__)(__VA_ARGS__))

/// Convenience method to add a class member with public read access but protected write access
#define POINTER_R(...) EXP(___DETAIL_POINTER_R(__VA_ARGS__)(__VA_ARGS__))

/// Convenience method to add a class member with public read access and write access
#define POINTER_RW(...) EXP(___DETAIL_POINTER_RW(__VA_ARGS__)(__VA_ARGS__))

/// Convenience method to add a class member with public read access but protected write access including a protected accessor
#define POINTER_R_IW(...) EXP(___DETAIL_POINTER_R_IW(__VA_ARGS__)(__VA_ARGS__))

/// Convenience method to add a class member with public read access but protected write access
#define REFERENCE_R(...) EXP(___DETAIL_REFERENCE_R(__VA_ARGS__)(__VA_ARGS__))

/// Convenience method to add a class member with public read access and write access
#define REFERENCE_RW(...) EXP(___DETAIL_REFERENCE_RW(__VA_ARGS__)(__VA_ARGS__))

/// This will only set properties internal to the actual class
#define PROPERTY_INTERNAL(...) EXP(___DETAIL_PROPERTY_RW_INTERNAL(__VA_ARGS__)(__VA_ARGS__))

/// This will only set properties internal to the actual class
#define VIRTUAL_PROPERTY_INTERNAL(...) EXP(___DETAIL_VIRTUAL_PROPERTY_RW_INTERNAL(__VA_ARGS__)(__VA_ARGS__))

/// This will only set properties internal to the actual class
#define POINTER_INTERNAL(...) EXP(___DETAIL_POINTER_RW_INTERNAL(__VA_ARGS__)(__VA_ARGS__))

/// This will only set properties internal to the actual class
#define REFERENCE_INTERNAL(...) EXP(___DETAIL_REFERENCE_RW_INTERNAL(__VA_ARGS__)(__VA_ARGS__))
#pragma endregion
}; //namespace Divide;

#endif //DVD_PLATFORM_DATA_TYPES_H_
