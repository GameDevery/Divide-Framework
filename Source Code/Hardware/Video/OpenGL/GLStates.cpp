//OpenGL state management: Lights, matrices, viewport, bound objects etc
#include "Headers/GLWrapper.h"

#include "Core/Headers/ParamHandler.h"
#include "Managers/Headers/ShaderManager.h"
#include "Rendering/Lighting/Headers/Light.h"
#include "Hardware/Video/Headers/GFXDevice.h"
#include "Hardware/Video/OpenGL/Buffers/VertexBuffer/Headers/glVertexArray.h"
#include "Hardware/Video/OpenGL/Buffers/ShaderBuffer/Headers/glUniformBuffer.h"

GLuint GL_API::_activeVAOId = GL_INVALID_INDEX;
GLuint GL_API::_activeFBId  = GL_INVALID_INDEX;
GLuint GL_API::_activeBufferId[] = {GL_INVALID_INDEX, GL_INVALID_INDEX, GL_INVALID_INDEX, GL_INVALID_INDEX, GL_INVALID_INDEX, GL_INVALID_INDEX, GL_INVALID_INDEX};
GLuint GL_API::_activeTextureUnit = GL_INVALID_INDEX;
GLuint GL_API::_activeTransformFeedback = GL_INVALID_INDEX;

bool GL_API::_lastRestartIndexSmall = true;
bool GL_API::_primitiveRestartEnabled = false;

GL_API::textureBoundMapDef GL_API::textureBoundMap;
GL_API::samplerBoundMapDef GL_API::samplerBoundMap;
Unordered_map<GLuint, vec4<GLfloat> > GL_API::_prevClearColor;

void GL_API::clearStates(const bool skipShader,const bool skipTextures,const bool skipBuffers, const bool forceAll){
    if(!skipShader || forceAll) {
        setActiveProgram(nullptr,forceAll);
    }

    if(!skipTextures || forceAll){
        FOR_EACH(textureBoundMapDef::value_type& it, textureBoundMap)
            GL_API::bindTexture(it.first, 0, it.second.second, 0);

        setActiveTextureUnit(0, forceAll);
    }

    if(!skipBuffers || forceAll){
        setActiveVAO(0,forceAll);
        setActiveFB(0, true, true, forceAll);
        setActiveBuffer(GL_ARRAY_BUFFER, 0, forceAll);
        setActiveBuffer(GL_TEXTURE_BUFFER, 0, forceAll);
        setActiveBuffer(GL_UNIFORM_BUFFER, 0, forceAll);
        setActiveBuffer(GL_SHADER_STORAGE_BUFFER, 0, forceAll);
        setActiveBuffer(GL_ELEMENT_ARRAY_BUFFER, 0, forceAll);
        setActiveBuffer(GL_PIXEL_UNPACK_BUFFER, 0, forceAll);
        setActiveBuffer(GL_DRAW_INDIRECT_BUFFER, 0, forceAll);
        setActiveTransformFeedback(0, forceAll);
    }

    GL_API::clearColor(DefaultColors::DIVIDE_BLUE());
}

void GL_API::togglePrimitiveRestart(bool state, bool smallIndices){
    if (_lastRestartIndexSmall != smallIndices && state){
        _lastRestartIndexSmall = smallIndices;
        glPrimitiveRestartIndex(smallIndices ? Config::PRIMITIVE_RESTART_INDEX_S : Config::PRIMITIVE_RESTART_INDEX_L);
    }
    if (_primitiveRestartEnabled != state){
        _primitiveRestartEnabled = state;
        state ? glEnable(GL_PRIMITIVE_RESTART) : glDisable(GL_PRIMITIVE_RESTART);
    }
}

void GL_API::updateClipPlanes(){
    GLuint clipPlaneIndex = 0;
    bool   clipPlaneActive = false;

    const PlaneList& list = GFX_DEVICE.getClippingPlanes();
    
    for (const Plane<F32>& clipPlane : list){
        clipPlaneIndex  = clipPlane.getIndex();
        clipPlaneActive = clipPlane.active();

        if (_activeClipPlanes[clipPlaneIndex] != clipPlaneActive){
            _activeClipPlanes[clipPlaneIndex]  = clipPlaneActive;
            clipPlaneActive ? glEnable(GL_CLIP_DISTANCE0 + clipPlaneIndex) : glDisable(GL_CLIP_DISTANCE0 + clipPlaneIndex);
        }
    }
}

bool GL_API::setActiveTextureUnit(GLuint unit,const bool force){
    if(_activeTextureUnit == unit && !force)
        return false; //< prevent double bind

    _activeTextureUnit = unit;
    glActiveTexture(GL_TEXTURE0 + unit);

    return true;
}

bool GL_API::bindSampler(GLuint unit, GLuint samplerID){
    if(samplerBoundMap[unit] != samplerID){
        glBindSampler(unit, samplerID);
        samplerBoundMap[unit] = samplerID;
        return true;
    }
    return false;
}
  
bool GL_API::bindTexture(GLuint unit, GLuint handle, GLenum type, GLuint samplerID){
    GL_API::bindSampler(unit, samplerID);

    std::pair<GLuint, GLenum>& currentMapping = textureBoundMap[unit];
    if (currentMapping.first != handle || currentMapping.second != type){
        GL_API::setActiveTextureUnit(unit);
        glBindTexture(type, handle);

        currentMapping.first = handle;
        currentMapping.second = type;
        return true;
    }
    return false;
}

bool GL_API::setActiveFB(GLuint id, const bool read, const bool write, const bool force){
    if (_activeFBId == id && !force)
        return false; //<prevent double bind

    _activeFBId = id;
    if (read && write)      glBindFramebuffer(GL_FRAMEBUFFER, id);
    else if(read && !write) glBindFramebuffer(GL_READ_FRAMEBUFFER, id);
    else                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, id);

    return true;
}

bool GL_API::setActiveVAO(GLuint id, const bool force){
    if(_activeVAOId == id && !force)
        return false; //<prevent double bind
        
    _activeVAOId = id;
    glBindVertexArray(id);

    return true;
}

bool GL_API::setActiveTransformFeedback(GLuint id, const bool force){
    if(_activeTransformFeedback == id && !force)
        return false;

    _activeTransformFeedback = id;
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, id);
    return true;
}

bool GL_API::setActiveBuffer(GLenum target, GLuint id, const bool force){
    GLint index = -1;
    switch (target){
        case GL_ARRAY_BUFFER          : index = 0; break;
        case GL_TEXTURE_BUFFER        : index = 1; break;
        case GL_UNIFORM_BUFFER        : index = 2; break;
        case GL_SHADER_STORAGE_BUFFER : index = 3; break;
        case GL_ELEMENT_ARRAY_BUFFER  : index = 4; break;
        case GL_PIXEL_UNPACK_BUFFER   : index = 5; break;
        case GL_DRAW_INDIRECT_BUFFER  : index = 6; break;
    };

    DIVIDE_ASSERT(index != -1, "GLStates error: attempted to bind an invalid buffer target!");

    if (_activeBufferId[index] == id && !force)
        return false; //<prevent double bind

    _activeBufferId[index] = id;
    glBindBuffer(target, id);

    return true;
}

bool GL_API::setActiveProgram(glShaderProgram* const program,const bool force){
    GLuint newProgramId = (program != nullptr) ? program->getId() : 0;
    GLuint oldProgramId = (GFXDevice::_activeShaderProgram != nullptr) ? GFXDevice::_activeShaderProgram->getId() : 0;
    if(oldProgramId == newProgramId && !force)
        return false; //<prevent double bind

    if (GFXDevice::_activeShaderProgram != nullptr) GFXDevice::_activeShaderProgram->unbind(false);

    GFXDevice::_activeShaderProgram = program;

    glUseProgram(newProgramId);
    return true;
}

void GL_API::clearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a, GLuint renderTarget, bool force){
    vec4<F32>& prevClearColor = _prevClearColor[renderTarget];
    if (prevClearColor != vec4<F32>(r,g,b,a)){
        prevClearColor.set(r, g, b, a);
        glClearColor(r,g,b,a);
    }
}

void GL_API::changeViewport(const vec4<I32>& newViewport) const {
    if (Config::Profile::USE_1x1_VIEWPORT)
        glViewport(newViewport.x, newViewport.y, 1, 1);
    else
        glViewport(newViewport.x, newViewport.y, newViewport.z, newViewport.w);

}

#ifndef SHOULD_TOGGLE
    #define SHOULD_TOGGLE(state) (!oldBlock || oldBlock->getDescriptor().state != newDescriptor.state)
    #define SHOULD_TOGGLE_2(state1, state2)  (!oldBlock ||SHOULD_TOGGLE(state1) || SHOULD_TOGGLE(state2))
    #define SHOULD_TOGGLE_3(state1, state2, state3) (!oldBlock ||SHOULD_TOGGLE_2(state1, state2) || SHOULD_TOGGLE(state3))
    #define SHOULD_TOGGLE_4(state1, state2, state3, state4) (!oldBlock || SHOULD_TOGGLE_3(state1, state2, state3) || SHOULD_TOGGLE(state4))
    #define TOGGLE_NO_CHECK(state, enumValue) newDescriptor.state ? glEnable(enumValue) : glDisable(enumValue)
    #define TOGGLE_WITH_CHECK(state, enumValue) if(SHOULD_TOGGLE(state)) TOGGLE_NO_CHECK(state, enumValue);
#endif

void GL_API::activateStateBlock(const RenderStateBlock& newBlock, RenderStateBlock* const oldBlock) const {
    // ------------------- PASS INDEPENDENT STATES -------------------------------------- //
    const RenderStateBlockDescriptor& newDescriptor = newBlock.getDescriptor();

    TOGGLE_WITH_CHECK(_blendEnable,   GL_BLEND);
    TOGGLE_WITH_CHECK(_cullMode,      GL_CULL_FACE);
    TOGGLE_WITH_CHECK(_stencilEnable, GL_STENCIL_TEST);
    TOGGLE_WITH_CHECK(_zEnable,       GL_DEPTH_TEST);

    if (SHOULD_TOGGLE_2(_blendSrc, _blendDest))
        glBlendFuncSeparate(glBlendTable[newDescriptor._blendSrc], glBlendTable[newDescriptor._blendDest], GL_ONE, GL_ZERO);

    if (SHOULD_TOGGLE(_blendOp))
        glBlendEquation(glBlendOpTable[newDescriptor._blendOp]);
    
    if (SHOULD_TOGGLE(_cullMode))
        glCullFace(glCullModeTable[newDescriptor._cullMode]);
    
    if (SHOULD_TOGGLE(_fillMode))
        glPolygonMode(GL_FRONT_AND_BACK, glFillModeTable[newDescriptor._fillMode]);
    
    if (SHOULD_TOGGLE(_zFunc))
        glDepthFunc(glCompareFuncTable[newDescriptor._zFunc]);
    
    if (SHOULD_TOGGLE(_zWriteEnable))
        glDepthMask(newDescriptor._zWriteEnable);
    
    if (SHOULD_TOGGLE(_stencilWriteMask))
        glStencilMask(newDescriptor._stencilWriteMask);
    
    if (SHOULD_TOGGLE_3(_stencilFunc, _stencilRef, _stencilMask))
        glStencilFunc(glCompareFuncTable[newDescriptor._stencilFunc], newDescriptor._stencilRef, newDescriptor._stencilMask);
    
    if (SHOULD_TOGGLE_3(_stencilFailOp, _stencilZFailOp, _stencilPassOp))
        glStencilOp(glStencilOpTable[newDescriptor._stencilFailOp], glStencilOpTable[newDescriptor._stencilZFailOp], glStencilOpTable[newDescriptor._stencilPassOp]);
    
    if (SHOULD_TOGGLE(_zBias)){
        if (IS_ZERO(newDescriptor._zBias)){
            glDisable(GL_POLYGON_OFFSET_FILL);
        }else {
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(newDescriptor._zBias, newDescriptor._zUnits);
        }
    }

    if (SHOULD_TOGGLE_4(_colorWrite.b.b0, _colorWrite.b.b1, _colorWrite.b.b2, _colorWrite.b.b3)) {
        glColorMask(newDescriptor._colorWrite.b.b0 == GL_TRUE, // R
                    newDescriptor._colorWrite.b.b1 == GL_TRUE, // G
                    newDescriptor._colorWrite.b.b2 == GL_TRUE, // B
                    newDescriptor._colorWrite.b.b3 == GL_TRUE);// A
    }
}