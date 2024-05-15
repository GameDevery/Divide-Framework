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
#ifndef DVD_PUSH_CONSTANT_H_
#define DVD_PUSH_CONSTANT_H_

namespace Divide::GFX
{
    struct PushConstant
    {
        template<typename T> requires (!std::is_same_v<bool, T>)
        PushConstant(const U64 bindingHash, const PushConstantType type, const T& data);
        template<typename T> requires (!std::is_same_v<bool, T>)
        PushConstant(const U64 bindingHash, const PushConstantType type, const T* data, const size_t count);

        const Byte* data() const noexcept;

        PROPERTY_R_IW(size_t, dataSize, 0u);
        PROPERTY_R_IW(U64, bindingHash, 0u);
        PROPERTY_R_IW(PushConstantType, type, PushConstantType::COUNT);

        bool operator==(const PushConstant& rhs) const = default;

    private:
        eastl::fixed_vector<Byte, 8, true> _buffer;
    };

} //namespace Divide::GFX

#endif //DVD_PUSH_CONSTANT_H_

#include "PushConstant.inl"
