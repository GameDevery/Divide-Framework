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
#ifndef DVD_CORE_DISPLAY_MANAGER_INL_
#define DVD_CORE_DISPLAY_MANAGER_INL_


namespace Divide
{
    inline bool operator==(const DisplayManager::OutputDisplayProperties& lhs, const DisplayManager::OutputDisplayProperties& rhs) noexcept
    {
        return lhs._resolution == rhs._resolution &&
               lhs._bitsPerPixel == rhs._bitsPerPixel &&
               lhs._formatName == rhs._formatName &&
               COMPARE(lhs._maxRefreshRate, rhs._maxRefreshRate);
    }
} //namespace Divide

#endif //DVD_CORE_DISPLAY_MANAGER_INL_