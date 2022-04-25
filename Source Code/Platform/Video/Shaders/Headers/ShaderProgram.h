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

#include "Core/Headers/ObjectPool.h"
#include "Core/Resources/Headers/Resource.h"
#include "Core/Resources/Headers/ResourceDescriptor.h"
#include "Platform/Video/Headers/GraphicsResource.h"
#include "Platform/Video/Headers/RenderAPIEnums.h"
#include "Platform/Video/Headers/PushConstant.h"

namespace FW {
    class FileWatcher;
};

namespace Divide {

class Kernel;
class Camera;
class Material;
class ResourceCache;
class ShaderProgramDescriptor;

struct PushConstants;
struct Configuration;

enum class FileUpdateEvent : U8;

FWD_DECLARE_MANAGED_CLASS(ShaderBuffer);
FWD_DECLARE_MANAGED_CLASS(ShaderProgram);

namespace Attorney {
    class ShaderProgramKernel;
}

struct ModuleDefine {
    ModuleDefine() = default;
    ModuleDefine(const char* define, const bool addPrefix = true) : ModuleDefine(string{ define }, addPrefix) {}
    ModuleDefine(const string& define, const bool addPrefix = true) : _define(define), _addPrefix(addPrefix) {}

    string _define;
    bool _addPrefix = true;
};

using ModuleDefines = vector<ModuleDefine>;

struct ShaderModuleDescriptor {
    ShaderModuleDescriptor() = default;
    explicit ShaderModuleDescriptor(ShaderType type, const Str64& file, const Str64& variant = "")
        : _moduleType(type), _sourceFile(file), _variant(variant)
    {
    }

    ModuleDefines _defines;
    Str64 _sourceFile;
    Str64 _variant;
    ShaderType _moduleType = ShaderType::COUNT;
};

struct PerFileShaderData;
class ShaderProgramDescriptor final : public PropertyDescriptor {
public:
    ShaderProgramDescriptor() noexcept
        : PropertyDescriptor(DescriptorType::DESCRIPTOR_SHADER) {

    }

    size_t getHash() const override;
    Str256 _name;
    ModuleDefines _globalDefines;
    vector<ShaderModuleDescriptor> _modules;
};

struct ShaderProgramMapEntry {
    ShaderProgram* _program = nullptr;
    U8 _generation = 0u;
};

inline bool operator==(const ShaderProgramMapEntry& lhs, const ShaderProgramMapEntry& rhs) noexcept {
    return lhs._generation == rhs._generation &&
           lhs._program == rhs._program;
}

inline bool operator!=(const ShaderProgramMapEntry& lhs, const ShaderProgramMapEntry& rhs) noexcept {
    return lhs._generation != rhs._generation ||
           lhs._program != rhs._program;
}

namespace Reflection {
    static constexpr U32 INVALID_BINDING_INDEX = std::numeric_limits<U32>::max();

    struct BlockMember {
        GFX::PushConstantType _type{ GFX::PushConstantType::COUNT };
        Str64 _name{};
        size_t _offset{ 0u };
        size_t _arrayInnerSize{ 0u }; // array[innerSize][outerSize]
        size_t _arrayOuterSize{ 0u }; // array[innerSize][outerSize]
        size_t _vectorDimensions{ 0u };
        vec2<size_t> _matrixDimensions{ 0u, 0u }; //columns, rows
    };

    struct Data {
        U32 _targetBlockBindingIndex = INVALID_BINDING_INDEX;
        string _targetBlockName;
        size_t _blockSize{ 0u };
        vector<BlockMember> _blockMembers{};
    };
};

class NOINITVTABLE ShaderProgram : public CachedResource,
                                   public GraphicsResource {
    friend class Attorney::ShaderProgramKernel;
   public:
    static constexpr char* UNIFORM_BLOCK_NAME = "dvd_uniforms";

    struct UniformDeclaration {
        Str64 _type;
        Str256 _name;
    };

    // one per shader type!
    struct LoadData {
        enum class SourceCodeSource : U8 {
            SOURCE_FILES,
            TEXT_CACHE,
            SPIRV_CACHE,
            COUNT
        };
        vector<UniformDeclaration> _uniforms;
        std::vector<U32> _sourceCodeSpirV;
        eastl::string _sourceCodeGLSL;
        Str256 _name = "";
        Str256 _fileName = "";
        size_t _definesHash = 0u;
        ShaderType _type = ShaderType::COUNT;
        SourceCodeSource _codeSource = SourceCodeSource::COUNT;
    };

    struct ShaderLoadData {
        string _uniformBlock{};
        Reflection::Data _reflectionData{};
        vector<LoadData> _data;
    };

    struct UniformBlockUploaderDescriptor {
        eastl::string _parentShaderName = "";
        Reflection::Data _reflectionData{};
    };

    struct UniformBlockUploader {
        struct BlockMember
        {
            Reflection::BlockMember _externalData;
            U64    _nameHash{ 0u };
            size_t _size{ 0 };
            size_t _elementSize{ 0 };
        };


        explicit UniformBlockUploader(GFXDevice& context, const UniformBlockUploaderDescriptor& descriptor);

        void uploadPushConstant(const GFX::PushConstant& constant, bool force = false) noexcept;
        void commit();
        void prepare();

    private:
        vector<Byte> _localDataCopy;
        vector<BlockMember> _blockMembers;
        ShaderBuffer_uptr _buffer = nullptr;
        UniformBlockUploaderDescriptor _descriptor;
        size_t _uniformBlockSizeAligned = 0u;
        bool _uniformBlockDirty = false;
        bool _needsQueueIncrement = false;

    };

    using Handle = PoolHandle;
    static constexpr Handle INVALID_HANDLE{ U16_MAX, U8_MAX };

    static bool s_UseBindlessTextures;

    struct TextDumpEntry
    {
        Str256 _name;
        string _sourceCode;
    };

     using ShaderProgramMap = std::array<ShaderProgramMapEntry, U16_MAX>;

    using AtomMap = ska::bytell_hash_map<U64 /*name hash*/, string>;
    //using AtomInclusionMap = ska::bytell_hash_map<U64 /*name hash*/, vector<ResourcePath>>;
    using AtomInclusionMap = hashMap<U64 /*name hash*/, vector<ResourcePath>>;
    using ShaderQueue = eastl::stack<ShaderProgram*, vector_fast<ShaderProgram*> >;

   public:
    explicit ShaderProgram(GFXDevice& context,
                           size_t descriptorHash,
                           const Str256& shaderName,
                           const Str256& shaderFileName,
                           const ResourcePath& shaderFileLocation,
                           ShaderProgramDescriptor descriptor,
                           ResourceCache& parentCache);

    virtual ~ShaderProgram();

    bool load() override;
    bool unload() override;

    inline bool recompile() {
        bool skipped = false;
        return recompile(skipped);
    }

    virtual bool recompile(bool& skipped);

    void uploadPushConstants(const PushConstants& constants);
    void preparePushConstants();

    //==================== static methods ===============================//
    static void Idle(PlatformContext& platformContext);
    [[nodiscard]] static ErrorCode OnStartup(ResourceCache* parentCache);
    [[nodiscard]] static bool OnShutdown();
    [[nodiscard]] static bool OnThreadCreated(const GFXDevice& gfx, const std::thread::id& threadID);
    /// Queue a shaderProgram recompile request
    static bool RecompileShaderProgram(const Str256& name);
    /// Remove a shaderProgram from the program cache
    static bool UnregisterShaderProgram(Handle shaderHandle);
    /// Add a shaderProgram to the program cache
    static void RegisterShaderProgram(ShaderProgram* shaderProgram);
    /// Find a specific shader program by handle.
    [[nodiscard]] static ShaderProgram* FindShaderProgram(Handle shaderHandle);

    static void RebuildAllShaders();

    [[nodiscard]] static vector<ResourcePath> GetAllAtomLocations();

    [[nodiscard]] static bool UseShaderTexCache() noexcept { return s_useShaderTextCache; }
    [[nodiscard]] static bool UseShaderBinaryCache() noexcept { return s_useShaderBinaryCache; }

    [[nodiscard]] static size_t DefinesHash(const ModuleDefines& defines);

    [[nodiscard]] static I32 ShaderProgramCount() noexcept { return s_shaderCount.load(std::memory_order_relaxed); }

    [[nodiscard]] const ShaderProgramDescriptor& descriptor() const noexcept { return _descriptor; }

    [[nodiscard]] const char* getResourceTypeName() const noexcept override { return "ShaderProgram"; }

    static void OnAtomChange(std::string_view atomName, FileUpdateEvent evt);
    static void ParseGLSLSource(Reflection::Data& reflectionDataInOut, LoadData& dataInOut, bool targetVulkan, bool reloadExisting);
    static bool SaveReflectionData(const ResourcePath& path, const ResourcePath& file, const Reflection::Data& reflectionDataIn);
    static bool LoadReflectionData(const ResourcePath& path, const ResourcePath& file, Reflection::Data& reflectionDataOut);
    static void QueueShaderWriteToFile(const string& sourceCode, const Str256& fileName);

    template<typename StringType> 
    inline static StringType DecorateFileName(const StringType& name) {
        if_constexpr(Config::Build::IS_DEBUG_BUILD) {
            return "DEBUG." + name;
        } else if_constexpr(Config::Build::IS_PROFILE_BUILD) {
            return "PROFILE." + name;
        } else {
            return "RELEASE." + name;
        }
    }

    PROPERTY_RW(bool, highPriority, true);
    PROPERTY_R_IW(Handle, handle, INVALID_HANDLE);

   protected:
     static void UseShaderTextCache(bool state) noexcept { if (s_useShaderBinaryCache) { state = false; } s_useShaderTextCache = state; }
     static void UseShaderBinaryCache(const bool state) noexcept { s_useShaderBinaryCache = state; if (state) { UseShaderTextCache(false); } }

   protected:
    /// Only 1 shader program per frame should be recompiled to avoid a lot of stuttering
    static ShaderQueue s_recompileQueue;
    /// Shader program cache
    static ShaderProgramMap s_shaderPrograms;

    struct LastRequestedShader {
        ShaderProgram* _program = nullptr;
        Handle _handle = INVALID_HANDLE;
    };
    static LastRequestedShader s_lastRequestedShaderProgram;
    static Mutex s_programLock;

protected:
    virtual void threadedLoad(bool reloadExisting);
    virtual bool reloadShaders(hashMap<U64, PerFileShaderData>& fileData, bool reloadExisting);
            void setGLSWPath(bool clearExisting);

    void onAtomChangeInternal(std::string_view atomName, FileUpdateEvent evt);

    void loadSourceCode(const ModuleDefines& defines, bool reloadExisting, LoadData& loadDataInOut);

    void initUniformUploader(const PerFileShaderData& loadData);
private:
    static const string& ShaderFileRead(const ResourcePath& filePath, const ResourcePath& atomName, bool recurse, vector<U64>& foundAtomIDsInOut, bool& wasParsed);
    static const string& ShaderFileReadLocked(const ResourcePath& filePath, const ResourcePath& atomName, bool recurse, vector<U64>& foundAtomIDsInOut, bool& wasParsed);

    static bool ShaderFileRead(const ResourcePath& filePath, const ResourcePath& fileName, eastl::string& sourceCodeOut);
    static bool ShaderFileWrite(const ResourcePath& filePath, const ResourcePath& fileName, const char* sourceCode);

    static void DumpShaderTextCacheToDisk(const TextDumpEntry& entry);

    static eastl::string GatherUniformDeclarations(const eastl::string& source, vector<UniformDeclaration>& foundUniforms);

    static eastl::string PreprocessIncludes(const ResourcePath& name,
                                        const eastl::string& source,
                                        I32 level,
                                        vector<U64>& foundAtomIDsInOut,
                                        bool lock);
   protected:
    template <typename T>
    friend class ImplResourceLoader;

    ResourceCache& _parentCache;
    const ShaderProgramDescriptor _descriptor;

   protected:
    vector<UniformBlockUploader> _uniformBlockBuffers;
    vector<U64> _usedAtomIDs;

   protected:
    static bool s_useShaderTextCache;
    static bool s_useShaderBinaryCache;
    static std::atomic_int s_shaderCount;

    static I64 s_shaderFileWatcherID;

    /// Shaders loaded from files are kept as atoms
    static SharedMutex s_atomLock;
    static AtomMap s_atoms;
    static AtomInclusionMap s_atomIncludes;

    //extra entry for "common" location
    static ResourcePath shaderAtomLocationPrefix[to_base(ShaderType::COUNT) + 1];
    static Str8 shaderAtomExtensionName[to_base(ShaderType::COUNT) + 1];
    static U64 shaderAtomExtensionHash[to_base(ShaderType::COUNT) + 1];
};

struct PerFileShaderData {
    string _programName{};
    vector<ShaderModuleDescriptor> _modules;
    ShaderProgram::ShaderLoadData _loadData;
};

namespace Attorney {
    class ShaderProgramKernel {
      protected:
        static void UseShaderTextCache(const bool state) noexcept {
            ShaderProgram::UseShaderTextCache(state);
        }

        static void UseShaderBinaryCache(const bool state) noexcept {
            ShaderProgram::UseShaderBinaryCache(state);
        }

        friend class Kernel;
    };
}

};  // namespace Divide
#endif //_SHADER_PROGRAM_H_