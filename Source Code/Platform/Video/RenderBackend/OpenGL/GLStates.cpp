#include "stdafx.h"

#include "Headers/GLWrapper.h"

#include "Core/Headers/Kernel.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/Headers/RenderStateBlock.h"
#include "Platform/Video/RenderBackend/OpenGL/Buffers/ShaderBuffer/Headers/glUniformBuffer.h"

namespace Divide {

/// The following static variables are used to remember the current OpenGL state
GLuint GL_API::s_UBOffsetAlignment = 0u;
GLuint GL_API::s_UBMaxSize = 0u;
GLuint GL_API::s_SSBOffsetAlignment = 0u;
GLuint GL_API::s_SSBMaxSize = 0u;
GLuint GL_API::s_dummyVAO = 0u;
GLuint GL_API::s_maxTextureUnits = 0;
GLuint GL_API::s_maxAttribBindings = 0u;
GLuint GL_API::s_maxFBOAttachments = 0u;
GLuint GL_API::s_anisoLevel = 0u;
bool GL_API::s_UseBindlessTextures = false;
SharedMutex GL_API::s_mipmapQueueSetLock;
eastl::unordered_set<GLuint> GL_API::s_mipmapQueue;
SharedMutex GL_API::s_mipmapCheckQueueSetLock;
eastl::unordered_set<GLuint> GL_API::s_mipmapCheckQueue;

vectorEASTL<GL_API::ResidentTexture> GL_API::s_residentTextures;

SharedMutex GL_API::s_samplerMapLock;
GL_API::SamplerObjectMap GL_API::s_samplerMap;
GLUtil::glVAOPool GL_API::s_vaoPool;
glHardwareQueryPool* GL_API::s_hardwareQueryPool = nullptr;

GLStateTracker& GL_API::getStateTracker() noexcept {
    return s_stateTracker;
}

GLUtil::GLMemory::DeviceAllocator& GL_API::getMemoryAllocator() noexcept {
    return s_memoryAllocator;
}

/// Reset as much of the GL default state as possible within the limitations given
void GL_API::clearStates(const DisplayWindow& window, GLStateTracker& stateTracker, const bool global) const {
    if (global) {
        stateTracker.bindTextures(0, s_maxTextureUnits - 1, TextureType::COUNT, nullptr, nullptr);
        stateTracker.setPixelPackUnpackAlignment();
        stateTracker._activePixelBuffer = nullptr;
    }

    stateTracker.setActiveVAO(0);
    stateTracker.setActiveBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    stateTracker.setActiveFB(RenderTarget::RenderTargetUsage::RT_READ_WRITE, 0);
    stateTracker._activeClearColour.set(window.clearColour());
    const U8 blendCount = to_U8(stateTracker._blendEnabled.size());
    for (U8 i = 0; i < blendCount; ++i) {
        stateTracker.setBlending(i, {});
    }
    stateTracker.setBlendColour({ 0u, 0u, 0u, 0u });

    const vec2<U16>& drawableSize = _context.getDrawableSize(window);
    stateTracker.setScissor({ 0, 0, drawableSize.width, drawableSize.height });

    stateTracker._activePipeline = nullptr;
    stateTracker._activeRenderTarget = nullptr;
    stateTracker.setActiveProgram(0u);
    stateTracker.setActiveShaderPipeline(0u);
    stateTracker.setStateBlock(RenderStateBlock::defaultHash());
}

bool GL_API::DeleteBuffers(const GLuint count, GLuint* buffers) {
    if (count > 0 && buffers != nullptr) {
        for (GLuint i = 0; i < count; ++i) {
            const GLuint crtBuffer = buffers[i];
            GLStateTracker& stateTracker = getStateTracker();
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

bool GL_API::DeleteVAOs(const GLuint count, GLuint* vaos) {
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

bool GL_API::DeleteFramebuffers(const GLuint count, GLuint* framebuffers) {
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

bool GL_API::DeleteShaderPrograms(const GLuint count, GLuint* programs) {
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

bool GL_API::DeleteShaderPipelines(const GLuint count, GLuint* programPipelines) {
    if (count > 0 && programPipelines != nullptr) {
        for (GLuint i = 0; i < count; ++i) {
            if (getStateTracker()._activeShaderPipeline == programPipelines[i]) {
                getStateTracker().setActiveShaderPipeline(0u);
            }
        }

        glDeleteProgramPipelines(count, programPipelines);
        memset(programPipelines, 0, count * sizeof(GLuint));
        return true;
    }
    return false;
}

bool GL_API::DeleteTextures(const GLuint count, GLuint* textures, const TextureType texType) {
    if (count > 0 && textures != nullptr) {
        
        for (GLuint i = 0; i < count; ++i) {
            const GLuint crtTex = textures[i];
            if (crtTex != 0) {
                GLStateTracker& stateTracker = getStateTracker();

                auto bindingIt = stateTracker._textureBoundMap[to_base(texType)];
                for (GLuint& handle : bindingIt) {
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

bool GL_API::DeleteSamplers(const GLuint count, GLuint* samplers) {
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