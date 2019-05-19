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
#ifndef _PLATFORM_VIDEO_OPENGLS_PROGRAM_H_
#define _PLATFORM_VIDEO_OPENGLS_PROGRAM_H_

#include "Platform/Video/RenderBackend/OpenGL/Headers/glResources.h"
#include "Platform/Video/Shaders/Headers/ShaderProgram.h"

namespace Divide {
class GL_API;
class glShader;
class glLockManager;
namespace Attorney {
    class GLAPIShaderProgram;
};
/// OpenGL implementation of the ShaderProgram entity
class glShaderProgram final : public ShaderProgram, public glObject {
    friend class Attorney::GLAPIShaderProgram;
   public:
    explicit glShaderProgram(GFXDevice& context,
                             size_t descriptorHash,
                             const stringImpl& name,
                             const stringImpl& resourceName,
                             const stringImpl& resourceLocation,
                             const ShaderProgramDescriptor& descriptor,
                             bool asyncLoad);
    ~glShaderProgram();

    static void initStaticData();
    static void destroyStaticData();
    static void onStartup(GFXDevice& context, ResourceCache& parentCache);
    static void onShutdown();
    static stringImpl decorateFileName(const stringImpl& name);

    /// Make sure this program is ready for deletion
    bool unload() noexcept override;
    /// Check every possible combination of flags to make sure this program can
    /// be used for rendering
    bool isValid() const override;

    /// Get the index of the specified subroutine name for the specified stage.
    /// Not cached!
    U32 GetSubroutineIndex(ShaderType type, const char* name) override;
    U32 GetSubroutineUniformCount(ShaderType type) override;

    void UploadPushConstant(const GFX::PushConstant& constant);
    void UploadPushConstants(const PushConstants& constants);

    static void onAtomChange(const char* atomName, FileUpdateEvent evt);
    static const stringImpl& shaderFileRead(const stringImpl& filePath, const stringImpl& atomName, bool recurse, U32 level, vector<stringImpl>& foundAtoms, bool& wasParsed);
    static const stringImpl& shaderFileReadLocked(const stringImpl& filePath, const stringImpl& atomName, bool recurse, U32 level, vector<stringImpl>& foundAtoms, bool& wasParsed);

    static void shaderFileRead(const stringImpl& filePath, const stringImpl& fileName, stringImpl& sourceCodeOut);
    static void shaderFileWrite(const stringImpl& filePath, const stringImpl& fileName, const char* sourceCode);
    static stringImpl preprocessIncludes(const stringImpl& name,
                                         const stringImpl& source,
                                         GLint level,
                                         vector<stringImpl>& foundAtoms,
                                         vector<stringImpl>& foundDefines,
                                         bool lock);

   protected:

    vector<stringImpl> loadSourceCode(ShaderType stage,
                                      const stringImpl& stageName,
                                      const stringImpl& extension,
                                      const stringImpl& header,
                                      U32 lineOffset,
                                      bool forceReParse,
                                      std::pair<bool, stringImpl>& sourceCodeOut);

    void validatePreBind();
    void validatePostBind();

    bool shouldRecompile() const override;
    bool recompileInternal() override;
    /// Creation of a new shader program. Pass in a shader token and use glsw to
    /// load the corresponding effects
    bool load() override;
    /// This should be called in the loading thread, but some issues are still
    /// present, and it's not recommended (yet)
    void threadedLoad(bool skipRegister);

    /// Returns true if at least one shader linked succesfully
    bool reloadShaders(bool reparseShaderSource);

    /// This is used to set all of the subroutine indices for the specified
    /// shader stage for this program
    void SetSubroutines(ShaderType type, const vector<U32>& indices) const;
    /// This works exactly like SetSubroutines, but for a single index.
    void SetSubroutine(ShaderType type, U32 index) const;
    /// Bind this shader program
    bool bind(bool& wasBound);
    /// Returns true if the shader is currently active
    bool isBound() const;

   private:
    GLuint _handle;
    bool _validationQueued;
    bool _validated;

    UseProgramStageMask _stageMask = UseProgramStageMask::GL_NONE_BIT;
    vectorEASTL<glShader*> _shaderStage;
    static std::array<U32, to_base(ShaderType::COUNT)> _lineOffset;
    const ShaderProgramDescriptor _descriptor;

    static I64 s_shaderFileWatcherID;

    /// Shaders loaded from files are kept as atoms
    static SharedMutex s_atomLock;
    static AtomMap s_atoms;

    //extra entry for "common" location
    static stringImpl shaderAtomLocationPrefix[to_base(ShaderType::COUNT) + 1];
    static stringImpl shaderAtomExtensionName[to_base(ShaderType::COUNT) + 1];
    static U64 shaderAtomExtensionHash[to_base(ShaderType::COUNT) + 1];
};

namespace Attorney {
    class GLAPIShaderProgram {
    private:
        static void setGlobalLineOffset(U32 offset) {
            glShaderProgram::_lineOffset.fill(offset);
        }
        static void addLineOffset(ShaderType stage, U32 offset) {
            glShaderProgram::_lineOffset[to_U32(stage)] += offset;
        }
        static bool bind(glShaderProgram& program, bool& wasBound) {
            return program.bind(wasBound);
        }
        static void queueValidation(glShaderProgram& program) {
            // After using the shader at least once, validate the shader if needed
            if (!program._validated) {
                program._validationQueued = true;
            }
        }

        static void SetSubroutines(glShaderProgram& program, ShaderType type, const vector<U32>& indices) {
            program.SetSubroutines(type, indices);
        }
        static void SetSubroutine(glShaderProgram& program, ShaderType type, U32 index) {
            program.SetSubroutine(type, index);
        }
        friend class Divide::GL_API;
    };
};  // namespace Attorney
};  // namespace Divide

#endif  //_PLATFORM_VIDEO_OPENGLS_PROGRAM_H_

#include "glShaderProgram.inl"
