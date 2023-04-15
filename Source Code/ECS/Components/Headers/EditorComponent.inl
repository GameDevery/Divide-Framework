/* Copyright (c) 2018 DIVIDE-Studio
Copyright (c) 2009 Ionut Cava

This file is part of DIVIDE Framework.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software
and associated documentation files (the "Software"), to deal in the Software
without restriction,
including without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the Software
is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE
OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#pragma once
#ifndef _SGN_EDITOR_COMPONENT_INL_
#define _SGN_EDITOR_COMPONENT_INL_

namespace Divide
{
    template<typename T>
    T* EditorComponentField::getPtr() const
    {
        if ( _dataGetter )
        {
            T* ret = nullptr;
            _dataGetter( ret );
            return ret;
        }
        return static_cast<T*>(_data);
    }

    template<typename T>
    T EditorComponentField::get() const
    {
        if ( _dataGetter )
        {
            T dataOut = {};
            _dataGetter( &dataOut );
            return dataOut;
        }

        return *static_cast<T*>(_data);
    }

    template<typename T>
    void EditorComponentField::get( T& dataOut ) const
    {
        if ( _dataGetter )
        {
            _dataGetter( &dataOut );
        }
        else
        {
            dataOut = *static_cast<T*>(_data);
        }
    }

    template<typename T>
    void EditorComponentField::set( const T& dataIn )
    {
        if ( _readOnly )
        {
            return;
        }

        if ( _dataSetter )
        {
            _dataSetter( &dataIn );
        }
        else if ( _data )
        {
            *static_cast<T*>(_data) = dataIn;
        }
        else
        {
            DIVIDE_UNEXPECTED_CALL();
        }
    }

    inline const char* EditorComponentField::getDisplayName( const U8 index ) const
    {
        if ( _displayNameGetter )
        {
            return _displayNameGetter( index );
        }
        return "Error: no name getter!";
    }

    inline bool EditorComponentField::supportsByteCount() const noexcept
    {
        return _basicType == PushConstantType::INT ||
               _basicType == PushConstantType::UINT ||
               _basicType == PushConstantType::IVEC2 ||
               _basicType == PushConstantType::IVEC3 ||
               _basicType == PushConstantType::IVEC4 ||
               _basicType == PushConstantType::UVEC2 ||
               _basicType == PushConstantType::UVEC3 ||
               _basicType == PushConstantType::UVEC4 ||
               _basicType == PushConstantType::IMAT2 ||
               _basicType == PushConstantType::IMAT3 ||
               _basicType == PushConstantType::IMAT4 ||
               _basicType == PushConstantType::UMAT2 ||
               _basicType == PushConstantType::UMAT3 ||
               _basicType == PushConstantType::UMAT4;
    }

    inline bool EditorComponentField::isMatrix() const noexcept
    {
        return _basicType == PushConstantType::IMAT2 ||
               _basicType == PushConstantType::IMAT3 ||
               _basicType == PushConstantType::IMAT4 ||
               _basicType == PushConstantType::UMAT2 ||
               _basicType == PushConstantType::UMAT3 ||
               _basicType == PushConstantType::UMAT4 ||
               _basicType == PushConstantType::DMAT2 ||
               _basicType == PushConstantType::DMAT3 ||
               _basicType == PushConstantType::DMAT4 ||
               _basicType == PushConstantType::MAT2 ||
               _basicType == PushConstantType::MAT3 ||
               _basicType == PushConstantType::MAT4;
    }
} // namespace Divide

#endif //_SGN_EDITOR_COMPONENT_INL_
