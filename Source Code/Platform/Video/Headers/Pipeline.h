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

#ifndef _PIPELINE_H_
#define _PIPELINE_H_

#include "Platform/Video/Shaders/Headers/ShaderProgram.h"

namespace Divide {

class PipelineDescriptor {
public:
    U8 _multiSampleCount = 0;
    size_t _stateHash = 0;
    ShaderProgram_ptr _shaderProgram = nullptr;

    size_t computeHash() const;

private: //internal
    size_t _hash;
}; //class PipelineDescriptor

class Pipeline {
public:
    Pipeline();
    Pipeline(const PipelineDescriptor& descriptor);
    ~Pipeline();

    void fromDescriptor(const PipelineDescriptor& descriptor);
    PipelineDescriptor toDescriptor() const;

    inline ShaderProgram* shaderProgram() const {
        return _shaderProgram.get();
    }

    inline size_t stateHash() const {
        return _stateHash;
    }

    inline U8 multiSampleCount() const{
        return _multiSampleCount;
    }

    bool operator==(const Pipeline &other) const;
    bool operator!=(const Pipeline &other) const;

private: //data
    size_t _descriptorHash;
    size_t _stateHash;
    U8 _multiSampleCount;
    ShaderProgram_ptr _shaderProgram;

}; //class Pipeline

}; //namespace Divide

#endif //_PIPELINE_H_

