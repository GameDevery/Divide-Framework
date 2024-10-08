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
    http://www.cplusplus.com/forum/lounge/27570/
*/


#pragma once
#ifndef DVD_GUID_WRAPPER_H_
#define DVD_GUID_WRAPPER_H_

namespace Divide {

/// Utility class that adds basic GUID management to objects
class GUIDWrapper {
   public:
    GUIDWrapper() noexcept : _guid(generateGUID()) {}
    GUIDWrapper([[maybe_unused]] const GUIDWrapper& old) noexcept : _guid(generateGUID()) { }
    GUIDWrapper(GUIDWrapper&& old) noexcept : _guid(old._guid) {}
    virtual ~GUIDWrapper() = default;

    [[nodiscard]] FORCE_INLINE I64 getGUID() const noexcept { return _guid; }

    static I64 generateGUID() noexcept;

    GUIDWrapper& operator=(const GUIDWrapper& old) = delete;
    GUIDWrapper& operator=(GUIDWrapper&& other) = delete;

   protected:
    const I64 _guid;
};

FORCE_INLINE bool operator==(const GUIDWrapper& lhs, const GUIDWrapper& rhs) noexcept
{
    return lhs.getGUID() == rhs.getGUID();
}

FORCE_INLINE bool operator!=(const GUIDWrapper& lhs, const GUIDWrapper& rhs) noexcept
{
    return lhs.getGUID() != rhs.getGUID();
}

[[nodiscard]]
FORCE_INLINE bool Compare(const GUIDWrapper* const lhs, const GUIDWrapper* const rhs) noexcept
{
    if (lhs != nullptr && rhs != nullptr)
    {
        return lhs->getGUID() == rhs->getGUID();
    }

    return (lhs == nullptr && rhs == nullptr);
}


}  // namespace Divide

#endif //DVD_GUID_WRAPPER_H_
