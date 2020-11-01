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


#ifndef _CORE_STRING_HELPER_INL_
#define _CORE_STRING_HELPER_INL_

#ifndef FMT_EXCEPTIONS
#define FMT_EXCEPTIONS 0
#endif

namespace Divide {
namespace Util {

template<typename T_vec, typename T_str>
typename std::enable_if<std::is_same<T_vec, vectorEASTL<T_str>>::value ||
                        std::is_same<T_vec, vectorEASTLFast<T_str>>::value, T_vec&>::type
Split(const char* input, const char delimiter, T_vec& elems) {
    if (input != nullptr) {
        {
            size_t i;
            const char* o = input;
            for (i = 0; input[i]; input[i] == delimiter ? i++ : *(input++)) {
                NOP();
            }
            elems.resize(i + 1);
            input = o;
        }

        stringImpl item;
        istringstreamImpl ss(input);
        size_t idx = 0;
        while (std::getline(ss, item, delimiter)) {
            elems[idx++] = MOV(item);
        }
    } else {
        elems.clear();
    }

    return elems;
}

template<typename T_str>
vectorEASTL<T_str>& Split(const char* input, char delimiter, vectorEASTL<T_str>& elems) {
    assert(input != nullptr);

    const T_str original(input);
    if (!original.empty()) {
        {
            size_t i;
            const char* o = input;
            for (i = 0; input[i]; input[i] == delimiter ? i++ : *input++) {
                NOP();
            }
            elems.resize(i + 1);
            input = o;
        }

        size_t idx = 0;
        typename T_str::const_iterator start = eastl::begin(original);
        typename T_str::const_iterator end = eastl::end(original);
        typename T_str::const_iterator next = eastl::find(start, end, delimiter);
        while (next != end) {
            elems[idx++] = { start, next };
            start = next + 1;
            next = eastl::find(start, end, delimiter);
        }
        elems[idx] = { start, next };
    } else {
        elems.clear();
    }

    return elems;
}

template<typename T_vec, typename T_str>
typename std::enable_if<std::is_same<T_vec, vectorEASTLFast<T_str>>::value ||
                        std::is_same<T_vec, vectorEASTL<T_str>>::value, T_vec>::type
Split(const char* input, const char delimiter) {
    T_vec elems;
    return Split<T_vec, T_str>(input, delimiter, elems);
}

template<typename T_str>
bool IsNumber(const T_str& s) {
    return IsNumber(s.c_str());
}

template<typename T_str>
void GetPermutations(std::string_view subject, vectorEASTL<T_str>& permutationContainer) {
    permutationContainer.clear();
    T_str tempCpy(subject);
    std::sort(std::begin(tempCpy), std::end(tempCpy));
    do {
        permutationContainer.push_back(tempCpy);
    } while (std::next_permutation(std::begin(tempCpy), std::end(tempCpy)));
}

template<size_t N, typename T_str>
bool ReplaceStringInPlace(T_str& subject, const std::array<std::string_view, N>& search, std::string_view replace, bool recursive) {
    bool ret = true;
    bool changed = true;
    while (changed) {
        changed = false;
        for (std::string_view s : search) {
            changed = ReplaceStringInPlace(subject, s, replace, recursive);
            ret = changed || ret;
            
        }
    }

    return ret;
}

template<size_t N, typename T_str>
T_str ReplaceString(std::string_view subject, const std::array<std::string_view, N>& search, std::string_view replace, bool recursive) {
    T_str ret{ subject };
    bool changed = true;
    while (changed) {
        changed = false;
        for (std::string_view s : search) {
            changed = ReplaceStringInPlace(ret, s, replace, recursive) || changed;
        }
    }

    return ret;
}

template<typename T_str>
bool ReplaceStringInPlace(T_str& subject,
                          const std::string_view search,
                          const std::string_view replace,
                          const bool recursive) {
    bool ret = false;
    bool changed = true;
    while (changed) {
        changed = false;

        size_t pos = 0;
        while ((pos = subject.find(search.data(), pos)) != T_str::npos) {
            subject.replace(pos, search.length(), replace.data());
            pos += replace.length();
            changed = true;
            ret = true;
        }

        if (!recursive) {
            break;
        }
    }

    return ret;
}

template<typename T_str>
T_str ReplaceString(std::string_view subject,
                    std::string_view search,
                    std::string_view replace,
                    bool recursive)
{
    T_str ret{ subject };
    ReplaceStringInPlace(ret, search, replace, recursive);
    return ret;
}

template<typename T_str>
T_str GetTrailingCharacters(const T_str& input, size_t count) {
    const size_t inputLength = input.length();
    count = std::min(inputLength, count);
    assert(count > 0);
    return input.substr(inputLength - count, inputLength).data();
}

template<typename T_str>
T_str GetStartingCharacters(const T_str& input, size_t count) {
    const size_t inputLength = input.length();
    count = std::min(inputLength, count);
    assert(count > 0);
    return input.substr(0, inputLength - count);
}


inline bool CompareIgnoreCase(const char* a, const char* b) noexcept {
    assert(a != nullptr && b != nullptr);
    return strcasecmp(a, b) == 0;
}

inline bool CompareIgnoreCase(const char* a, const std::string_view b) noexcept {
    if (a != nullptr && !b.empty()) {
        return strncasecmp(a, b.data(), b.length()) == 0;
    } else if (b.empty()) {
        return Util::IsEmptyOrNull(a);
    }

    return false;
}

template<typename T_strA>
bool CompareIgnoreCase(const T_strA& a, const char* b) noexcept {
    if (b != nullptr && !a.empty()) {
        return CompareIgnoreCase(a.c_str(), b);
    } else if (a.empty()) {
        return Util::IsEmptyOrNull(b);
    }
    return false;
}

template<typename T_strA>
bool CompareIgnoreCase(const T_strA& a, std::string_view b) noexcept {
    return CompareIgnoreCase(a.c_str(), b);
}

template<>
inline bool CompareIgnoreCase(const stringImpl& a, const stringImpl& b) noexcept {
    if (a.length() == b.length()) {
        return std::equal(std::cbegin(b),
                          std::cend(b),
                          std::cbegin(a),
            [](const unsigned char a, const unsigned char b) noexcept {
                return a == b || std::tolower(a) == std::tolower(b);
            });
    }

    return false;
}

template<>
inline bool CompareIgnoreCase(const stringImplFast& a, const stringImplFast& b) noexcept {
    if (a.length() == b.length()) {
        return std::equal(std::cbegin(b),
                          std::cend(b),
                          std::cbegin(a),
            [](const unsigned char a, const unsigned char b) noexcept {
                return a == b || std::tolower(a) == std::tolower(b);
            });
    }

    return false;
}

template<typename T_strA, typename T_strB>
inline bool CompareIgnoreCase(const T_strA& a, const T_strB& b) noexcept {
    return CompareIgnoreCase(a.c_str(), b.c_str());
}

template<typename T_str>
U32 LineCount(const T_str& str) {
    return to_U32(std::count(std::cbegin(str), std::cend(str), '\n')) + 1;
}

/// http://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring
template<typename T_str>
inline T_str Ltrim(const T_str& s) {
    T_str temp(s);
    return Ltrim(temp);
}

template<typename T_str>
inline T_str& Ltrim(T_str& s) {
    s.erase(eastl::begin(s),
            eastl::find_if(eastl::begin(s),
                         eastl::end(s),
                         [](const char c) { return !std::isspace(c); }));
    return s;
}

template<typename T_str>
T_str Rtrim(const T_str& s) {
    T_str temp(s);
    return Rtrim(temp);
}

template<typename T_str>
T_str& Rtrim(T_str& s) {
    s.erase(eastl::find_if(eastl::rbegin(s),
                         eastl::rend(s),
                         [](const char c) { return !std::isspace(c); }).base(),
            eastl::end(s));
    return s;
}

template<typename T_str>
T_str& Trim(T_str& s) {
    return Ltrim(Rtrim(s));
}

template<typename T_str>
T_str Trim(const T_str& s) {
    T_str temp(s);
    return Trim(temp);
}

template<typename T>
stringImpl to_string(T value) {
    return fmt::format("{}", value);
}

}; //namespace Util
}; //namespace Divide
#endif //_CORE_STRING_HELPER_INL_