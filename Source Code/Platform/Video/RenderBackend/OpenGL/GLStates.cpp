#include "stdafx.h"

#include "config.h"

#include "Headers/GLWrapper.h"

#include "Core/Headers/Kernel.h"
#include "Core/Headers/Configuration.h"
#include "Core/Headers/PlatformContext.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/Headers/RenderStateBlock.h"
#include "Platform/Video/RenderBackend/OpenGL/Buffers/VertexBuffer/Headers/glVertexArray.h"
#include "Platform/Video/RenderBackend/OpenGL/Buffers/ShaderBuffer/Headers/glUniformBuffer.h"

namespace Divide {

/// The following static variables are used to remember the current OpenGL state
GLuint GL_API::s_UBOffsetAlignment = 0;
GLuint GL_API::s_UBMaxSize = 0;
GLuint GL_API::s_SSBOffsetAlignment = 0;
GLuint GL_API::s_SSBMaxSize = 0;
GLint  GL_API::s_lineWidthLimit = 1;
GLuint GL_API::s_dummyVAO = 0;
GLint  GL_API::s_maxTextureUnits = 0;
GLint  GL_API::s_maxAttribBindings = 0;
GLint  GL_API::s_maxFBOAttachments = 0;
GLuint GL_API::s_anisoLevel = 0;

SharedMutex GL_API::s_mipmapQueueSetLock;
eastl::unordered_set<GLuint> GL_API::s_mipmapQueue;

GL_API::SamplerObjectMap GL_API::s_samplerMap;
Mutex GL_API::s_samplerMapLock;
GLUtil::glVAOPool GL_API::s_vaoPool;
glHardwareQueryPool* GL_API::s_hardwareQueryPool = nullptr;

GLStateTracker& GL_API::getStateTracker() {
    return s_stateTracker;
}

/// Reset as much of the GL default state as possible within the limitations given
void GL_API::clearStates(const DisplayWindow& window, GLStateTracker& stateTracker, bool global) {
    static BlendingProperties defaultBlend = {};

    if (global) {
        stateTracker.bindTextures(0, s_maxTextureUnits, nullptr, nullptr, nullptr);
        stateTracker.setPixelPackUnpackAlignment();
        stateTracker.setActiveBuffer(GL_TEXTURE_BUFFER, 0);
        stateTracker.setActiveBuffer(GL_UNIFORM_BUFFER, 0);
        stateTracker.setActiveBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        stateTracker.setActiveBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        stateTracker.setActiveBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
        stateTracker._commandBufferOffset = 0u;
        stateTracker._activePixelBuffer = nullptr;
        //stateTracker._activeViewport.set(-1);
    }
    stateTracker._previousStateBlockHash = 0;
    stateTracker.setActiveVAO(0);
    stateTracker.setActiveBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    stateTracker.setActiveFB(RenderTarget::RenderTargetUsage::RT_READ_WRITE, 0);
    stateTracker._activeClearColour.set(window.clearColour());
    const size_t blendCount = stateTracker._blendEnabled.size();
    for (size_t i = 0; i < blendCount; ++i) {
        stateTracker.setBlending(to_U32(i), defaultBlend);
    }
    stateTracker.setBlendColour(UColour4(0u), true);

    const vec2<U16>& drawableSize = _context.getDrawableSize(window);
    stateTracker.setScissor(Rect<I32>(0, 0, drawableSize.width, drawableSize.height));

    stateTracker._activePipeline = nullptr;
    stateTracker._activeRenderTarget = nullptr;
    stateTracker.setActivePipeline(0u);
}

bool GL_API::deleteBuffers(GLuint count, GLuint* buffers) {
    if (count > 0 && buffers != nullptr) {
        for (GLuint i = 0; i < count; ++i) {
            const GLuint crtBuffer = buffers[i];
            GLStateTracker& stateTracker = GL_API::getStateTracker();
            for (GLuint& boundBuffer : stateTracker._activeBufferID) {
                if (boundBuffer == crtBuffer) {
                    boundBuffer = GLUtil::k_invalidObjectID;
                }
            }
            for (auto& boundBuffer : stateTracker._activeVAOIB) {
                if (boundBuffer.second == crtBuffer) {
                    boundBuffer.second = GLUtil::k_invalidObjectID;
                }
            }
        }

        glDeleteBuffers(count, buffers);
        memset(buffers, 0, count * sizeof(GLuint));
        return true;
    }

    return false;
}

bool GL_API::deleteVAOs(GLuint count, GLuint* vaos) {
    if (count > 0 && vaos != nullptr) {
        for (GLuint i = 0; i < count; ++i) {
            if (getStateTracker()._activeVAOID == vaos[i]) {
                getStateTracker()._activeVAOID = GLUtil::k_invalidObjectID;
                break;
            }
        }
        glDeleteVertexArrays(count, vaos);
        memset(vaos, 0, count * sizeof(GLuint));
        return true;
    }
    return false;
}

bool GL_API::deleteFramebuffers(GLuint count, GLuint* framebuffers) {
    if (count > 0 && framebuffers != nullptr) {
        for (GLuint i = 0; i < count; ++i) {
            const GLuint crtFB = framebuffers[i];
            for (GLuint& activeFB : getStateTracker()._activeFBID) {
                if (activeFB == crtFB) {
                    activeFB = GLUtil::k_invalidObjectID;
                }
            }
        }
        glDeleteFramebuffers(count, framebuffers);
        memset(framebuffers, 0, count * sizeof(GLuint));
        return true;
    }
    return false;
}

bool GL_API::deleteShaderPrograms(GLuint count, GLuint* programs) {
    if (count > 0 && programs != nullptr) {
        for (GLuint i = 0; i < count; ++i) {
            if (getStateTracker()._activeShaderProgram == programs[i]) {
                getStateTracker().setActiveProgram(0u);
            }
            glDeleteProgram(programs[i]);
        }
        
        memset(programs, 0, count * sizeof(GLuint));
        return true;
    }
    return false;
}

bool GL_API::deleteShaderPipelines(GLuint count, GLuint* programPipelines) {
    if (count > 0 && programPipelines != nullptr) {
        for (GLuint i = 0; i < count; ++i) {
            if (getStateTracker()._activeShaderPipeline == programPipelines[i]) {
                getStateTracker().setActivePipeline(0);
            }
        }

        glDeleteProgramPipelines(count, programPipelines);
        memset(programPipelines, 0, count * sizeof(GLuint));
        return true;
    }
    return false;
}

bool GL_API::deleteTextures(GLuint count, GLuint* textures, TextureType texType) {
    if (count > 0 && textures != nullptr) {
        
        for (GLuint i = 0; i < count; ++i) {
            const GLuint crtTex = textures[i];
            if (crtTex != 0) {
                GLStateTracker& stateTracker = getStateTracker();

                for (auto bindingIt : stateTracker._textureBoundMap) {
                    U32& handle = bindingIt[to_base(texType)];
                    if (handle == crtTex) {
                        handle = 0u;
                    }
                }

                for (ImageBindSettings& settings : stateTracker._imageBoundMap) {
                    if (settings._texture == crtTex) {
                        settings.reset();
                    }
                }
            }
        }
        glDeleteTextures(count, textures);
        memset(textures, 0, count * sizeof(GLuint));
        return true;
    }

    return false;
}

bool GL_API::deleteSamplers(GLuint count, GLuint* samplers) {
    if (count > 0 && samplers != nullptr) {

        for (GLuint i = 0; i < count; ++i) {
            const GLuint crtSampler = samplers[i];
            if (crtSampler != 0) {
                for (GLuint& boundSampler : getStateTracker()._samplerBoundMap) {
                    if (boundSampler == crtSampler) {
                        boundSampler = 0;
                    }
                }
            }
        }
        glDeleteSamplers(count, samplers);
        memset(samplers, 0, count * sizeof(GLuint));
        return true;
    }

    return false;
}

};