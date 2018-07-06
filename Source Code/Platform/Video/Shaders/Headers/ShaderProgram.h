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
#ifndef _SHADER_PROGRAM_H_
#define _SHADER_PROGRAM_H_

#include "config.h"

#include "Core/Resources/Headers/Resource.h"
#include "Platform/Video/Headers/GraphicsResource.h"
#include "Platform/Video/Headers/RenderAPIEnums.h"

#include <stack>

namespace FW {
    class FileWatcher;
};

namespace Divide {

class Kernel;
class Camera;
class Material;
class ShaderBuffer;
class PushConstants;
struct GenericDrawCommand;

enum class FileUpdateEvent : U8;

FWD_DECLARE_MANAGED_CLASS(ShaderProgram);

namespace Attorney {
    class ShaderProgramKernel;
}

class NOINITVTABLE ShaderProgram : public CachedResource,
                                   public GraphicsResource {
    friend class Attorney::ShaderProgramKernel;

   public:
    typedef std::tuple<ShaderProgram_wptr, U32, size_t> ShaderProgramMapEntry;
    typedef vector<ShaderProgramMapEntry> ShaderProgramMap;
    typedef hashMap<U64, stringImpl> AtomMap;
    typedef std::stack<ShaderProgram_ptr, vector<ShaderProgram_ptr> > ShaderQueue;
    /// A list of built-in sampler slots. Use these if possible
    enum class TextureUsage : U8 {
        UNIT0 = 0,
        UNIT1 = 1,
        NORMALMAP = 2,
        OPACITY = 3,
        SPECULAR = 4,
        PROJECTION = 5,
        DEPTH = 6,
        DEPTH_PREV = 7,
        REFLECTION_PLANAR = 8,
        REFRACTION_PLANAR = 9,
        REFLECTION_CUBE = 10,
        REFRACTION_CUBE = 11,
        SHADOW_CUBE = 12,
        SHADOW_LAYERED = 13,
        SHADOW_SINGLE = 14,
        COUNT,

        GLOSS = SPECULAR,
        ROUGHNESS = GLOSS
    };

   public:
    explicit ShaderProgram(GFXDevice& context,
                           size_t descriptorHash,
                           const stringImpl& name,
                           const stringImpl& resourceName,
                           const stringImpl& resourceLocation,
                           bool asyncLoad);
    virtual ~ShaderProgram();

    /// Is the shader ready for drawing?
    virtual bool isValid() const = 0;
    virtual bool update(const U64 deltaTimeUS);
    virtual bool load(const DELEGATE_CBK<void, CachedResource_wptr>& onLoadCallback) override;
    virtual bool unload() override;

    // Return the binding address (or other identifier) for the push constant specified by name
    virtual I32 Binding(const char* name) = 0;

    /// Subroutine
    virtual U32 GetSubroutineIndex(ShaderType type, const char* name) const = 0;
    virtual U32 GetSubroutineUniformLocation(ShaderType type, const char* name) const = 0;
    virtual U32 GetSubroutineUniformCount(ShaderType type) const = 0;

    /// ShaderProgram object id (i.e.: for OGL _shaderProgramID = glCreateProgram())
    inline U32 getID() const { return _shaderProgramID; }
    ///  Calling recompile will re-create the marked shaders from source files
    ///  and update them in the ShaderManager if needed
    bool recompile();
    /// Add a define to the shader. The defined must not have been added
    /// previously
    void addShaderDefine(const stringImpl& define);
    /// Remove a define from the shader. The defined must have been added
    /// previously
    void removeShaderDefine(const stringImpl& define);

    /** ------ BEGIN EXPERIMENTAL CODE ----- **/
    inline vec_size getFunctionCount(ShaderType shader, U8 LoD) {
        return _functionIndex[to_U32(shader)][LoD].size();
    }

    inline void setFunctionCount(ShaderType shader,
                                 U8 LoD,
                                 vec_size count) {
        _functionIndex[to_U32(shader)][LoD].resize(count, 0);
    }

    inline void setFunctionCount(ShaderType shader, vec_size count) {
        for (U8 i = 0; i < Config::SCENE_NODE_LOD; ++i) {
            setFunctionCount(shader, i, count);
        }
    }

    inline void setFunctionIndex(ShaderType shader,
                                 U8 LoD,
                                 U32 index,
                                 U32 functionEntry) {
        const U32 shaderTypeValue = to_U32(shader);

        if (_functionIndex[shaderTypeValue][LoD].empty()) {
            return;
        }

        DIVIDE_ASSERT(index < _functionIndex[shaderTypeValue][LoD].size(),
                      "ShaderProgram error: Invalid function index specified "
                      "for update!");
        if (_availableFunctionIndex[shaderTypeValue].empty()) {
            return;
        }

        DIVIDE_ASSERT(
            functionEntry < _availableFunctionIndex[shaderTypeValue].size(),
            "ShaderProgram error: Specified function entry does not exist!");
        _functionIndex[shaderTypeValue][LoD][index] =
            _availableFunctionIndex[shaderTypeValue][functionEntry];
    }

    inline U32 addFunctionIndex(ShaderType shader, U32 index) {
        U32 shaderTypeValue = to_U32(shader);

        _availableFunctionIndex[shaderTypeValue].push_back(index);
        return U32(_availableFunctionIndex[shaderTypeValue].size() - 1);
    }
    /** ------ END EXPERIMENTAL CODE ----- **/

    //==================== static methods ===============================//
    static void idle();
    static void onStartup(GFXDevice& context, ResourceCache& parentCache);
    static void onShutdown();
    static bool updateAll(const U64 deltaTimeUS);
    /// Queue a shaderProgram recompile request
    static bool recompileShaderProgram(const stringImpl& name);
    static const stringImpl& shaderFileRead(const stringImpl& filePath, const stringImpl& atomName);
    static void shaderFileRead(const stringImpl& filePath, const stringImpl& fileName, stringImpl& sourceCodeOut);
    static void shaderFileWrite(const stringImpl& filePath, const stringImpl& fileName, const char* sourceCode);
    /// Remove a shaderProgram from the program cache
    static bool unregisterShaderProgram(size_t shaderHash);
    /// Add a shaderProgram to the program cache
    static void registerShaderProgram(const ShaderProgram_ptr& shaderProgram);
    static ShaderProgram_wptr findShaderProgram(U32 shaderHandle);
    static ShaderProgram_wptr findShaderProgram(size_t shaderHash);

    /// Return a default shader used for general purpose rendering
    static const ShaderProgram_ptr& defaultShader();

    static const ShaderProgram_ptr& nullShader();

    static void onAtomChange(const char* atomName, FileUpdateEvent evt);

    static void rebuildAllShaders();

    static vector<stringImpl> getAllAtomLocations();

   protected:
     virtual bool recompileInternal() = 0;
     void registerAtomFile(const stringImpl& atomFile);

     static void useShaderTextCache(bool state) { if (s_useShaderBinaryCache) { state = false; } s_useShaderTextCache = state; }
     static void useShaderBinaryCache(bool state) { s_useShaderBinaryCache = state; if (state) { useShaderTextCache(false); } }

   protected:
    /// Shaders loaded from files are kept as atoms
    static SharedLock s_atomLock;
    static AtomMap s_atoms;
    /// Used to render geometry without valid materials.
    /// Should emulate the basic fixed pipeline functions (no lights, just colour and texture)
    static ShaderProgram_ptr s_imShader;
    /// Pointer to a shader that we will perform operations on
    static ShaderProgram_ptr s_nullShader;
    /// Only 1 shader program per frame should be recompiled to avoid a lot of stuttering
    static ShaderQueue s_recompileQueue;
    /// Shader program cache
    static ShaderProgramMap s_shaderPrograms;

    static SharedLock s_programLock;

   protected:
    template <typename T>
    friend class ImplResourceLoader;
    bool _shouldRecompile;
    bool _asyncLoad;
    std::atomic_bool _linked;
    U32 _shaderProgramID;  //<not thread-safe. Make sure assignment is protected
    // with a mutex or something
    /// A list of preprocessor defines
    vector<stringImpl> _definesList;
    /// A list of atoms used by this program. (All stages are added toghether)
    vector<stringImpl> _usedAtoms;

    static bool s_useShaderTextCache;
    static bool s_useShaderBinaryCache;

   private:
    std::array<std::array<vector<U32>, Config::SCENE_NODE_LOD>, to_base(ShaderType::COUNT)> _functionIndex;
    std::array<vector<U32>, to_base(ShaderType::COUNT)>  _availableFunctionIndex;

    static I64 s_shaderFileWatcherID;
};

namespace Attorney {
    class ShaderProgramKernel {
      protected:
        static void useShaderTextCache(bool state) { 
            ShaderProgram::useShaderTextCache(state);
        }

        static void useShaderBinaryCache(bool state) {
            ShaderProgram::useShaderBinaryCache(state);
        }

        friend class Divide::Kernel;
    };
}

};  // namespace Divide
#endif //_SHADER_PROGRAM_H_
