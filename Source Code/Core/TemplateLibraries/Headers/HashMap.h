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
#ifndef _HASH_MAP_H_
#define _HASH_MAP_H_

#include "TemplateAllocator.h"
#include <algorithm>

template<class T>
struct EnumHash;

template <typename Key>
using HashType = EnumHash<Key>;

#if defined(HASH_MAP_IMP) && HASH_MAP_IMP == BOOST_IMP
#include <boost/Unordered_Map.hpp>
// macros are evil but we need to extend namespaces so aliases don't work as well
#define hashAlg boost
#elif defined(HASH_MAP_IMP) && HASH_MAP_IMP == EASTL_IMP
#include <EASTL/unordered_map.h>
#include <EASTL/intrusive_hash_map.h>
#define hashAlg eastl
#else  // defined(HASH_MAP_IMP) && HASH_MAP_IMP == STL_IMP
#include <unordered_map>
#define hashAlg std
#endif


template <typename K, typename V, typename HashFun = HashType<K> >
using hashMap = hashAlg::unordered_map<K, V, HashFun>;

template <typename K, typename V, typename HashFun = HashType<K> >
using hashPairReturn = hashAlg::pair<typename hashMap<K, V, HashFun>::iterator, bool>;

template <typename K, typename V>
using hashMapIntrusive = hashAlg::intrusive_hash_map<K, V, 37>;

template<class T, bool>
struct hasher {
    inline size_t operator() (const T& elem) noexcept {
        return hashAlg::hash<T>()(elem);
    }
};

template<class T>
struct hasher<T, true> {
    inline size_t operator() (const T& elem) {
        typedef typename std::underlying_type<T>::type enumType;
        return hashAlg::hash<enumType>()(static_cast<enumType>(elem));
    }
};

template<class T>
struct EnumHash {
    inline size_t operator()(const T& elem) const {
        return hasher<T, hashAlg::is_enum<T>::value>()(elem);
    }
};

namespace hashAlg {

template <> struct hash<std::string>
{
    size_t operator()(const std::string& x) const
    {
        const char* p = x.c_str();
        uint32_t c, result = 2166136261U;   // Intentionally uint32_t instead of size_t, so the behavior is the same regardless of size.
        while ((c = (uint8_t)*p++) != 0)     // cast to unsigned 8 bit.
            result = (result * 16777619) ^ c;
        return (size_t)result;
    }
};


template <typename K, typename V, typename ... Args, typename HashFun = HashType<K> >
inline hashPairReturn<K, V, HashFun> emplace(hashMap<K, V, HashFun>& map, K key, Args&&... args) {
    return hashAlg::insert(map, key, std::move(V(std::forward<Args>(args)...)));
}

template <typename K, typename V, typename HashFun = HashType<K> >
inline hashPairReturn<K, V, HashFun> insert(hashMap<K, V, HashFun>& map, const hashAlg::pair<K, V>& valuePair) {
    return map.insert(hashAlg::pair<K, V>(valuePair.first, valuePair.second));
}

template <typename K, typename V, typename HashFun = HashType<K> >
inline hashPairReturn<K, V, HashFun> insert(hashMap<K, V, HashFun>& map, K key, const V& value) {
    return hashAlg::insert(map, hashAlg::pair<K, V>(key, value));
}

template <typename K, typename V, typename HashFun = HashType<K> >
inline hashPairReturn<K, V, HashFun> insert(hashMap<K, V, HashFun>& map, K key, V&& value) {
    return hashAlg::insert(map, hashAlg::pair<K, V>(key, value));
}

}; //namespace hashAlg

#endif
