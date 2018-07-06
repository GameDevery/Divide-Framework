/*
   Copyright (c) 2015 DIVIDE-Studio
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

#ifndef GL_UNIFORM_BUFFER_OBJECT_H_
#define GL_UNIFORM_BUFFER_OBJECT_H_

#include "Platform/Video/OpenGL/Headers/glResources.h"
#include "Platform/Video/OpenGL/Buffers/Headers/glMemoryManager.h"
#include "Platform/Video/Buffers/ShaderBuffer/Headers/ShaderBuffer.h"

namespace Divide {

/// Base class for shader uniform blocks
class glBufferLockManager;
class glUniformBuffer final : public ShaderBuffer {
   public:
     glUniformBuffer(const stringImpl &bufferName,
                     const U32 sizeFactor, 
                     bool unbound,
                     bool persistentMapped);
    ~glUniformBuffer();

    void Destroy();
    /// Create a new buffer object to hold our uniform shader data
    void Create(U32 primitiveCount, ptrdiff_t primitiveSize);

    void UpdateData(GLintptr offsetElementCount,
                    GLsizeiptr rangeElementCount,
                    const bufferPtr data);

    bool BindRange(U32 bindIndex,
                   U32 offsetElementCount,
                   U32 rangeElementCount);

    bool Bind(U32 bindIndex);


   protected:
    void PrintInfo(const ShaderProgram *shaderProgram, U32 bindIndex);

   protected:
    GLuint _UBOid;
    GLsizeiptr _alignmentPadding;
    bufferPtr _mappedBuffer;
    bool _updated;
    const GLenum _target;
    const std::unique_ptr<glBufferLockManager> _lockManager;
};

};  // namespace Divide

#endif