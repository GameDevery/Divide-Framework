/*
Copyright (c) 2017 DIVIDE-Studio
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

#ifndef _PLATFORM_FILE_FILE_MANAGEMENT_INL_
#define _PLATFORM_FILE_FILE_MANAGEMENT_INL_

#include <fstream>

namespace Divide {

template<typename T>
bool readFile(const stringImpl& filePath, T& contentOut, FileType fileType) {
    static_assert(std::is_same<decltype(has_assign<T>(nullptr)), std::true_type>::value,
                  "Specified target container does not have a direct assignment operator!");

    size_t fileSize = 0;
    if (!filePath.empty()) {
        std::ifstream streamIn(filePath.c_str(),
                               fileType == FileType::BINARY
                                         ? std::ios::in | std::ios::binary
                                         : std::ios::in);

        if (!streamIn.eof() && !streamIn.fail()) {
            streamIn.seekg(0, std::ios::end);
            fileSize = streamIn.tellg();
            streamIn.seekg(0, std::ios::beg);

            optional_reserve(contentOut, fileSize);
            contentOut.assign((std::istreambuf_iterator<Byte>(streamIn)),
                               std::istreambuf_iterator<Byte>());
        }

        streamIn.close();
    };

    return fileSize > 0;
}

//Optimized variant for vectors
template<>
inline bool readFile(const stringImpl& filePath, vectorImpl<Byte>& contentOut, FileType fileType) {
    size_t fileSize = 0;
    if (!filePath.empty()) {
        std::ifstream streamIn(filePath.c_str(),
                               fileType == FileType::BINARY
                                         ? std::ios::in | std::ios::binary
                                         : std::ios::in);

        if (!streamIn.eof() && !streamIn.fail()) {
            streamIn.seekg(0, std::ios::end);
            fileSize = streamIn.tellg();
            streamIn.seekg(0, std::ios::beg);

            contentOut.resize(fileSize);
            streamIn.read(contentOut.data(), fileSize);
        }

        streamIn.close();
    };

    return fileSize > 0;
}
}; //namespace divide
#endif //_PLATFORM_FILE_FILE_MANAGEMENT_INL_