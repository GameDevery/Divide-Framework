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
#ifndef DVD_GL_RESOURCES_H_
#define DVD_GL_RESOURCES_H_

#include <glbinding/gl/gl.h>

struct SDL_Window;
typedef void* SDL_GLContext;

namespace NS_GLIM {
    enum class GLIM_ENUM : int;
}; //namespace NS_GLIM

namespace Divide {

struct GenericDrawCommand;

class DisplayWindow;
class glBufferImpl;

struct BufferLockEntry
{
    glBufferImpl* _buffer = nullptr;
    size_t _offset = 0u;
    size_t _length = 0u;
};

struct ImageBindSettings 
{
    gl::GLuint _texture = 0;
    gl::GLint  _level = 0;
    gl::GLint _layer = 0;
    gl::GLenum _access = gl::GL_NONE;
    gl::GLenum _format = gl::GL_NONE;
    gl::GLboolean _layered = gl::GL_FALSE;

    bool operator==( const ImageBindSettings&) const = default;
};

class VAOBindings
{
public:
    struct BufferBindingParams 
    {
        gl::GLuint _id{0u};
        size_t _offset{0u};
        size_t _stride{0u};

        bool operator==( const BufferBindingParams&) const = default;
    };

private:
    using VAOBufferData = vector_fast<BufferBindingParams>;
    using VAODivisors = vector_fast<bool>;
    using VAOData = std::pair<VAOBufferData, VAODivisors>;

public:
    void init(U32 maxBindings) noexcept;

    const BufferBindingParams& bindingParams( gl::GLuint index);

    bool instanceDivisorFlag( gl::GLuint index);
    void instanceDivisorFlag( gl::GLuint index, bool perInstanceDivisor);

    void bindingParams( gl::GLuint index, const BufferBindingParams& newParams);

private:

    VAOData _bindings;
    U32 _maxBindings = 0u;
};

/// Invalid object value. Used to compare handles and determine if they were properly created
constexpr gl::GLuint GL_NULL_HANDLE = gl::GL_INVALID_INDEX;

namespace GLUtil {

// Not thread-safe!
class glTextureViewCache 
{
private:
    enum class State : U8 {
        USED = 0,
        FREE,
        CLEAN
    };
public:
    void onFrameEnd();
    void init(U32 poolSize);
    void destroy();

    std::pair<gl::GLuint, bool> allocate(size_t hash, bool retry = false);
    // no-hash version
    gl::GLuint allocate(bool retry = false);
    void deallocate( gl::GLuint handle, U32 frameDelay = 1);

private:
    vector<State>  _usageMap;

    vector<U32>        _lifeLeft;
    vector<gl::GLuint> _handles;
    vector<gl::GLuint> _tempBuffer;

    hashMap<size_t, U32> _cache;

    //Heavy-handed general purpose lock
    SharedMutex _lock;
};

/// Wrapper for glGetIntegerv
template<typename T = gl::GLint>
void getGLValue(gl::GLenum param, T& value, gl::GLint index = -1);

template<typename T = gl::GLint>
void getGLValue( gl::GLenum param, T* value);

template<typename T = gl::GLint>
T getGLValue( gl::GLenum param);

template<typename T = gl::GLint>
T getGLValueIndexed( gl::GLenum param, gl::GLint index = -1);

/// Check the current operation for errors
void DebugCallback( gl::GLenum source, gl::GLenum type, gl::GLuint id, gl::GLenum severity,
                    gl::GLsizei length, const gl::GLchar* message, const void* userParam);


extern gl::GLuint s_lastQueryResult;
extern const DisplayWindow* s_glMainRenderWindow;
extern thread_local SDL_GLContext s_glSecondaryContext;


extern Mutex s_glSecondaryContextMutex;

///Note: If internal format is not GL_NONE, an indexed draw is issued!
void SubmitRenderCommand(const GenericDrawCommand& drawCommand, bool useIndirectBuffer, gl::GLenum internalFormat);

/// Populate enumeration tables with appropriate API values
void OnStartup();

struct FormatAndDataType
{
    gl::GLenum _format{ gl::GL_NONE};
    gl::GLenum _internalFormat{ gl::GL_NONE};
    gl::GLenum _dataType{ gl::GL_NONE};
};

FormatAndDataType InternalFormatAndDataType(GFXImageFormat baseFormat, GFXDataFormat dataType, GFXImagePacking packing) noexcept;
gl::GLenum internalTextureType(TextureType type, U8 msaaSamples);

extern std::array<gl::GLenum, to_base(BlendProperty::COUNT)> glBlendTable;
extern std::array<gl::GLenum, to_base(BlendOperation::COUNT)> glBlendOpTable;
extern std::array<gl::GLenum, to_base(ComparisonFunction::COUNT)> glCompareFuncTable;
extern std::array<gl::GLenum, to_base(StencilOperation::COUNT)> glStencilOpTable;
extern std::array<gl::GLenum, to_base(CullMode::COUNT)> glCullModeTable;
extern std::array<gl::GLenum, to_base(FillMode::COUNT)> glFillModeTable;
extern std::array<gl::GLenum, to_base(TextureType::COUNT)> glTextureTypeTable;
extern std::array<gl::GLenum, to_base(PrimitiveTopology::COUNT)> glPrimitiveTypeTable;
extern std::array<gl::GLenum, to_base(GFXDataFormat::COUNT)> glDataFormatTable;
extern std::array<gl::GLenum, to_base(TextureWrap::COUNT)> glWrapTable;
extern std::array<gl::GLenum, to_base(ShaderType::COUNT)> glShaderStageTable;
extern std::array<gl::GLenum, to_base( QueryType::COUNT )> glQueryTypeTable;

};  // namespace GLUtil
};  // namespace Divide

#endif //DVD_GL_RESOURCES_H_

#include "glResources.inl"
