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
#ifndef _GL_STATE_TRACKER_H_
#define _GL_STATE_TRACKER_H_

#include "glResources.h"

namespace Divide {
    static const U32 MAX_ACTIVE_TEXTURE_SLOTS = 64;

    class Pipeline;
    class glFramebuffer;
    class glPixelBuffer;
    class RenderStateBlock;
    class glBufferLockManager;

    struct GLStateTracker {
      public:
        void init(GLStateTracker* base);

        /// Enable or disable primitive restart and ensure that the correct index size is used
        void togglePrimitiveRestart(bool state);
        /// Enable or disable primitive rasterization
        void toggleRasterization(bool state);
        /// Set the number of vertices per patch used in tessellation
        void setPatchVertexCount(U32 count);
        /// Switch the currently active vertex array object
        bool setActiveVAO(GLuint ID);
        /// Switch the currently active vertex array object
        bool setActiveVAO(GLuint ID, GLuint& previousID);
        /// Single place to change buffer objects for every target available
        bool setActiveBuffer(GLenum target, GLuint ID);
        /// Single place to change buffer objects for every target available
        bool setActiveBuffer(GLenum target, GLuint ID, GLuint& previousID);
        /// Switch the current framebuffer by binding it as either a R/W buffer, read
        /// buffer or write buffer
        bool setActiveFB(RenderTarget::RenderTargetUsage usage, GLuint ID);
        /// Set a new depth range. Default is 0 - 1 with 0 mapping to the near plane and 1 to the far plane
        void setDepthRange(F32 nearVal, F32 farVal);
        void setBlending(const BlendingProperties& blendingProperties, bool force = false);
        inline void setBlending(bool force = false) {
            setBlending(_blendPropertiesGlobal, force);
        }
        /// Set the blending properties for the specified draw buffer
        void setBlending(GLuint drawBufferIdx, const BlendingProperties& blendingProperties, bool force = false);

        inline void setBlending(GLuint drawBufferIdx, bool force = false) {
            setBlending(drawBufferIdx, _blendProperties[drawBufferIdx], force);
        }

        void setBlendColour(const UColour& blendColour, bool force = false);
        /// Switch the current framebuffer by binding it as either a R/W buffer, read
        /// buffer or write buffer
        bool setActiveFB(RenderTarget::RenderTargetUsage usage, GLuint ID, GLuint& previousID);
        /// Change the currently active shader program.
        bool setActiveProgram(GLuint programHandle);
        /// A state block should contain all rendering state changes needed for the next draw call.
        /// Some may be redundant, so we check each one individually
        void activateStateBlock(const RenderStateBlock& newBlock,
            const RenderStateBlock& oldBlock);
        void activateStateBlock(const RenderStateBlock& newBlock);
        /// Pixel pack and unpack alignment is usually changed by textures, PBOs, etc
        bool setPixelPackUnpackAlignment(GLint packAlignment = 4, GLint unpackAlignment = 4) {
            return (setPixelPackAlignment(packAlignment) && setPixelUnpackAlignment(unpackAlignment));
        }
        /// Pixel pack alignment is usually changed by textures, PBOs, etc
        bool setPixelPackAlignment(GLint packAlignment = 4, GLint rowLength = 0, GLint skipRows = 0, GLint skipPixels = 0);
        /// Pixel unpack alignment is usually changed by textures, PBOs, etc
        bool setPixelUnpackAlignment(GLint unpackAlignment = 4, GLint rowLength = 0, GLint skipRows = 0, GLint skipPixels = 0);
        /// Bind a texture specified by a GL handle and GL type to the specified unit
        /// using the sampler object defined by handle value
        bool bindTexture(GLushort unit, GLuint handle, GLuint samplerHandle = 0u);
        bool bindTextureImage(GLushort unit, GLuint handle, GLint level,
            bool layered, GLint layer, GLenum access,
            GLenum format);
        /// Bind multiple textures specified by an array of handles and an offset unit
        bool bindTextures(GLushort unitOffset, GLuint textureCount, GLuint* textureHandles, GLuint* samplerHandles);

        size_t setStateBlock(size_t stateBlockHash);

        /// Bind multiple samplers described by the array of hash values to the
        /// consecutive texture units starting from the specified offset
        bool bindSamplers(GLushort unitOffset, GLuint samplerCount, GLuint* samplerHandles);

        /// Modify buffer bindings for a specific vao
        bool bindActiveBuffer(GLuint vaoID,
                              GLuint location,
                              GLuint bufferID,
                              GLintptr offset,
                              GLsizei stride);

        bool setScissor(I32 x, I32 y, I32 width, I32 height);

        inline bool setScissor(const Rect<I32>& newScissorRect) {
            return setScissor(newScissorRect.x, newScissorRect.y, newScissorRect.z, newScissorRect.w);
        }

        bool setClearColour(const FColour& colour);
        inline bool setClearColour(const UColour& colour) {
            return setClearColour(Util::ToFloatColour(colour));
        }

        /// Return the OpenGL framebuffer handle bound and assigned for the specified usage
        inline GLuint getActiveFB(RenderTarget::RenderTargetUsage usage) {
            return _activeFBID[to_U32(usage)];
        }

        /// Change the current viewport area. Redundancy check is performed in GFXDevice class
        bool setViewport(I32 x, I32 y, I32 width, I32 height);

        GLuint getBoundTextureHandle(GLuint slot);

        void getActiveViewport(GLint* vp);

      public:
        Pipeline const* _activePipeline = nullptr;
        glFramebuffer*  _activeRenderTarget = nullptr;
        glPixelBuffer*  _activePixelBuffer = nullptr;
        /// Current active vertex array object's handle
        GLuint _activeVAOID = GLUtil::_invalidObjectID;
        /// 0 - current framebuffer, 1 - current read only framebuffer, 2 - current write only framebuffer
        GLuint _activeFBID[3] = { GLUtil::_invalidObjectID,
                                  GLUtil::_invalidObjectID,
                                  GLUtil::_invalidObjectID };
        /// VB, IB, SB, TB, UB, PUB, DIB
        GLuint _activeBufferID[6] = { GLUtil::_invalidObjectID,
                                      GLUtil::_invalidObjectID,
                                      GLUtil::_invalidObjectID,
                                      GLUtil::_invalidObjectID,
                                      GLUtil::_invalidObjectID,
                                      GLUtil::_invalidObjectID };
        hashMap<GLuint, GLuint> _activeVAOIB;

        GLint  _activePackUnpackAlignments[2] = { 1 , 1 };
        GLint  _activePackUnpackRowLength[2] = { 0 , 0 };
        GLint  _activePackUnpackSkipPixels[2] = { 0 , 0 };
        GLint  _activePackUnpackSkipRows[2] = {0 , 0};
        GLuint _activeShaderProgram = 0; //GLUtil::_invalidObjectID;
        GLfloat _depthNearVal = .1f;
        GLfloat _depthFarVal = 1.f;
        BlendingProperties _blendPropertiesGlobal = { BlendProperty::ONE,
                                                      BlendProperty::ONE,
                                                      BlendOperation::ADD };
        GLboolean _blendEnabledGlobal = GL_FALSE;

        vector<BlendingProperties> _blendProperties;
        vector<GLboolean> _blendEnabled;

        UColour   _blendColour = UColour(0u);
        Rect<I32> _activeViewport = Rect<I32>(-1);
        Rect<I32> _activeScissor = Rect<I32>(-1);
        FColour   _activeClearColour = DefaultColours::DIVIDE_BLUE;

        /// Boolean value used to verify if primitive restart index is enabled or disabled
        bool _primitiveRestartEnabled = false;
        bool _rasterizationEnabled = true;
        U32  _patchVertexCount = 4;

        size_t _currentStateBlockHash = 0;
        size_t _previousStateBlockHash = 0;

        /// /*texture slot*/ /*texture handle*/
        typedef std::array<GLuint, MAX_ACTIVE_TEXTURE_SLOTS> textureBoundMapDef;
        textureBoundMapDef _textureBoundMap;

        typedef std::array<ImageBindSettings, MAX_ACTIVE_TEXTURE_SLOTS> imageBoundMapDef;
        imageBoundMapDef _imageBoundMap;

        /// /*texture slot*/ /*sampler handle*/
        typedef std::array<GLuint, MAX_ACTIVE_TEXTURE_SLOTS> samplerBoundMapDef;
        samplerBoundMapDef _samplerBoundMap;

        VAOBindings _vaoBufferData;
        bool _opengl46Supported = false;

      private:
        bool _init = false;
    }; //class GLStateTracker
}; //namespace Divide


#endif //_GL_STATE_TRACKER_H_