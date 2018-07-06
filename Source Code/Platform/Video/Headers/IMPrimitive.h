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

#ifndef _IM_EMULATION_H_
#define _IM_EMULATION_H_

#include "Utility/Headers/Colours.h"
#include "Core/Math/Headers/Line.h"
#include "Core/Math/Headers/MathMatrices.h"
#include "Platform/Video/Headers/Pipeline.h"
#include "Platform/Video/Headers/GraphicsResource.h"
#include "Platform/Video/Buffers/VertexBuffer/Headers/VertexDataInterface.h"

namespace Divide {

class Texture;
enum class PrimitiveType : U32;

FWD_DECLARE_MANAGED_CLASS(IMPrimitive);

/// IMPrimitive replaces immediate mode calls to VB based rendering
class NOINITVTABLE IMPrimitive : public VertexDataInterface {
   public:
    inline void setRenderStates(const DELEGATE_CBK<void>& setupStatesCallback,
                                const DELEGATE_CBK<void>& releaseStatesCallback) {
        _setupStates = setupStatesCallback;
        _resetStates = releaseStatesCallback;
    }

    inline void clearRenderStates() {
        _setupStates = nullptr;
        _resetStates = nullptr;
    }

    inline void setupStates() {
        if (_setupStates) {
            _setupStates();
        }
    }

    inline void resetStates() {
        if (_resetStates) {
            _resetStates();
        }
    }

    inline const Pipeline& pipeline() const {
        return _pipeline;
    }

    virtual void pipeline(const Pipeline& pipeline);

    virtual void draw(const GenericDrawCommand& command) = 0;

    inline I32 getLastDrawPrimitiveCount() const override {
        return _primitivesGenerated;
    }

    virtual void beginBatch(bool reserveBuffers, 
                            unsigned int vertexCount,
                            unsigned int attributeCount) = 0;

    virtual void begin(PrimitiveType type) = 0;
    virtual void vertex(F32 x, F32 y, F32 z) = 0;
    inline void vertex(const vec3<F32>& vert) {
        vertex(vert.x, vert.y, vert.z);
    }
    virtual void attribute1i(U32 attribLocation, I32 value) = 0;
    virtual void attribute1f(U32 attribLocation, F32 value) = 0;
    virtual void attribute4ub(U32 attribLocation, U8 x, U8 y, U8 z,  U8 w) = 0;
    virtual void attribute4f(U32 attribLocation, F32 x, F32 y, F32 z, F32 w) = 0;
    inline void attribute4ub(U32 attribLocation, const vec4<U8>& value) {
        attribute4ub(attribLocation, value.x, value.y, value.z, value.w);
    }
    inline void attribute4f(U32 attribLocation, const vec4<F32>& value) {
        attribute4f(attribLocation, value.x, value.y, value.z, value.w);
    }

    virtual void end() = 0;
    virtual void endBatch() = 0;

    void clear();

    inline void paused(bool state) { _paused = state; }
    inline bool paused() const { return _paused; }
    inline void forceWireframe(bool state) { _forceWireframe = state; }
    inline bool forceWireframe() const { return _forceWireframe; }
    inline bool hasRenderStates() const {
        return (!_setupStates && !_resetStates);
    }

    inline const mat4<F32>& worldMatrix() const { return _worldMatrix; }
    inline void worldMatrix(const mat4<F32>& worldMatrix) {
        _worldMatrix.set(worldMatrix);
    }
    inline void resetWorldMatrix() { _worldMatrix.identity(); }

    inline void name(const stringImpl& name) {
#       ifdef _DEBUG
        _name = name;
#       else
        ACKNOWLEDGE_UNUSED(name);
#       endif
    }

    virtual GenericDrawCommand toDrawCommand() const = 0;

    void fromBox(const vec3<F32>& min,
                 const vec3<F32>& max,
                 const vec4<U8>& colour = DefaultColours::WHITE);
    void fromSphere(const vec3<F32>& center,
                    F32 radius,
                    const vec4<U8>& colour = DefaultColours::WHITE);
    void fromLines(const vectorImpl<Line>& lines);
    void fromLines(const vectorImpl<Line>& lines,
                   const vec4<I32>& viewport);
   protected:
    void fromLines(const vectorImpl<Line>& lines,
                   const vec4<I32>& viewport,  //<only for ortho mode
                   const bool inViewport);
   protected:
    IMPrimitive(GFXDevice& context);
#ifdef _DEBUG
    stringImpl _name;
#endif
   public:
    virtual ~IMPrimitive();

   public:

    Pipeline _pipeline;
    Texture* _texture;

   protected:
    I32 _primitivesGenerated;
    // render in wireframe mode
    bool _forceWireframe;

   private:
    /// If _pause is true, rendering for the current primitive is skipped and
    /// nothing is modified (e.g. zombie counters)
    bool _paused;
    /// 2 functions used to setup or reset states
    DELEGATE_CBK<void> _setupStates;
    DELEGATE_CBK<void> _resetStates;
    /// The transform matrix for this element
    mat4<F32> _worldMatrix;
};

};  // namespace Divide

#endif