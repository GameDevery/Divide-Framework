#include "stdafx.h"

#include "Headers/Material.h"
#include "Headers/ShaderComputeQueue.h"

#include "Managers/Headers/SceneManager.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/Headers/RenderStateBlock.h"
#include "Utility/Headers/Localization.h"

#include "Core/Headers/Configuration.h"
#include "Core/Headers/Kernel.h"
#include "Core/Resources/Headers/ResourceCache.h"
#include "ECS/Components/Headers/RenderingComponent.h"
#include "Editor/Headers/Editor.h"
#include "Rendering/RenderPass/Headers/NodeBufferedData.h"

namespace Divide {

namespace {
    constexpr size_t g_materialXMLVersion = 1;

    constexpr size_t g_invalidStateHash = std::numeric_limits<size_t>::max();
};


namespace TypeUtil {
    const char* MaterialDebugFlagToString(const MaterialDebugFlag materialFlag) noexcept {
        return Names::materialDebugFlag[to_base(materialFlag)];
    }

    MaterialDebugFlag StringToMaterialDebugFlag(const string& name) {
        for (U8 i = 0; i < to_U8(MaterialDebugFlag::COUNT); ++i) {
            if (strcmp(name.c_str(), Names::materialDebugFlag[i]) == 0) {
                return static_cast<MaterialDebugFlag>(i);
            }
        }

        return MaterialDebugFlag::COUNT;
    } 
    
    const char* TextureUsageToString(const TextureUsage texUsage) noexcept {
        return Names::textureUsage[to_base(texUsage)];
    }

    TextureUsage StringToTextureUsage(const string& name) {
        for (U8 i = 0; i < to_U8(TextureUsage::COUNT); ++i) {
            if (strcmp(name.c_str(), Names::textureUsage[i]) == 0) {
                return static_cast<TextureUsage>(i);
            }
        }

        return TextureUsage::COUNT;
    }

    const char* BumpMethodToString(const BumpMethod bumpMethod) noexcept {
        return Names::bumpMethod[to_base(bumpMethod)];
    }

    BumpMethod StringToBumpMethod(const string& name) {
        for (U8 i = 0; i < to_U8(BumpMethod::COUNT); ++i) {
            if (strcmp(name.c_str(), Names::bumpMethod[i]) == 0) {
                return static_cast<BumpMethod>(i);
            }
        }

        return BumpMethod::COUNT;
    }

    const char* ShadingModeToString(const ShadingMode shadingMode) noexcept {
        return Names::shadingMode[to_base(shadingMode)];
    }

    ShadingMode StringToShadingMode(const string& name) {
        for (U8 i = 0; i < to_U8(ShadingMode::COUNT); ++i) {
            if (strcmp(name.c_str(), Names::shadingMode[i]) == 0) {
                return static_cast<ShadingMode>(i);
            }
        }

        return ShadingMode::COUNT;
    }

    const char* TextureOperationToString(const TextureOperation textureOp) noexcept {
        return Names::textureOperation[to_base(textureOp)];
    }

    TextureOperation StringToTextureOperation(const string& operation) {
        for (U8 i = 0; i < to_U8(TextureOperation::COUNT); ++i) {
            if (strcmp(operation.c_str(), Names::textureOperation[i]) == 0) {
                return static_cast<TextureOperation>(i);
            }
        }

        return TextureOperation::COUNT;
    }
}; //namespace TypeUtil

SamplerAddress Material::s_defaultTextureAddress = 0u;

void Material::ApplyDefaultStateBlocks(Material& target) {
    /// Normal state for final rendering
    RenderStateBlock stateDescriptor = {};
    stateDescriptor.setCullMode(target.doubleSided() ? CullMode::NONE : CullMode::BACK);
    stateDescriptor.setZFunc(ComparisonFunction::EQUAL);

    /// the z-pre-pass descriptor does not process colours
    RenderStateBlock zPrePassDescriptor(stateDescriptor);
    zPrePassDescriptor.setZFunc(ComparisonFunction::LEQUAL);

    RenderStateBlock depthPassDescriptor(zPrePassDescriptor);
    depthPassDescriptor.setColourWrites(false, false, false, false);

    /// A descriptor used for rendering to depth map
    RenderStateBlock shadowDescriptor(stateDescriptor);
    shadowDescriptor.setColourWrites(true, true, false, false);
    shadowDescriptor.setZFunc(ComparisonFunction::LESS);

    target.setRenderStateBlock(depthPassDescriptor.getHash(), RenderStage::COUNT,  RenderPassType::PRE_PASS);
    target.setRenderStateBlock(zPrePassDescriptor.getHash(),  RenderStage::DISPLAY,RenderPassType::PRE_PASS);
    target.setRenderStateBlock(stateDescriptor.getHash(),     RenderStage::COUNT,  RenderPassType::MAIN_PASS);
    target.setRenderStateBlock(stateDescriptor.getHash(),     RenderStage::COUNT,  RenderPassType::OIT_PASS);
    target.setRenderStateBlock(shadowDescriptor.getHash(),    RenderStage::SHADOW, RenderPassType::COUNT);
}

void Material::OnStartup(const SamplerAddress defaultTexAddress) {
    s_defaultTextureAddress = defaultTexAddress;
}

Material::Material(GFXDevice& context, ResourceCache* parentCache, const size_t descriptorHash, const Str256& name)
    : CachedResource(ResourceType::DEFAULT, descriptorHash, name),
      _useBindlessTextures(context.context().config().rendering.useBindlessTextures),
      _debugBindlessTextures(context.context().config().rendering.debugBindlessTextures),
      _context(context),
      _parentCache(parentCache)
{
    _useForPrePass.fill(TexturePrePassUsage::AUTO);

    receivesShadows(_context.context().config().rendering.shadowMapping.enabled);

    _textureAddresses.fill(s_defaultTextureAddress);
    _textureOperations.fill(TextureOperation::NONE);

    const ShaderProgramInfo defaultShaderInfo = {};
    // Could just direct copy the arrays, but this looks cool
    for (U8 s = 0u; s < to_U8(RenderStage::COUNT); ++s) {
        ShaderVariantsPerPass& perPassInfo = _shaderInfo[s];
        StateVariantsPerPass& perPassStates = _defaultRenderStates[s];

        for (U8 p = 0u; p < to_U8(RenderPassType::COUNT); ++p) {
            perPassInfo[p].fill(defaultShaderInfo);
            perPassStates[p].fill(g_invalidStateHash);
        }
    }

    ApplyDefaultStateBlocks(*this);
}

Material_ptr Material::clone(const Str256& nameSuffix) {
    DIVIDE_ASSERT(!nameSuffix.empty(),
                  "Material error: clone called without a valid name suffix!");

    const Material& base = *this;
    Material_ptr cloneMat = CreateResource<Material>(_parentCache, ResourceDescriptor(resourceName() + nameSuffix.c_str()));

    {
        cloneMat->_baseMaterial = this;
        _instances.emplace_back(cloneMat.get());
    }

    cloneMat->_baseColour = base._baseColour;
    cloneMat->_emissive = base._emissive;
    cloneMat->_ambient = base._ambient;
    cloneMat->_specular = base._specular;
    cloneMat->_specGloss = base._specGloss;
    cloneMat->_metallic = base._metallic;
    cloneMat->_roughness = base._roughness;
    cloneMat->_parallaxFactor = base._parallaxFactor;
    cloneMat->_shadingMode = base._shadingMode;
    cloneMat->_translucencySource = base._translucencySource;
    cloneMat->_textureOperations = base._textureOperations;
    cloneMat->_bumpMethod = base._bumpMethod;
    cloneMat->_doubleSided = base._doubleSided;
    cloneMat->_transparencyEnabled = base._transparencyEnabled;
    cloneMat->_useAlphaDiscard = base._useAlphaDiscard;
    cloneMat->_receivesShadows = base._receivesShadows;
    cloneMat->_isStatic = base._isStatic;
    cloneMat->_ignoreTexDiffuseAlpha = base._ignoreTexDiffuseAlpha;
    cloneMat->_hardwareSkinning = base._hardwareSkinning;
    cloneMat->_isRefractive = base._isRefractive;
    cloneMat->_extraShaderDefines = base._extraShaderDefines;
    cloneMat->_customShaderCBK = base._customShaderCBK;
    cloneMat->_useForPrePass = base._useForPrePass;
    cloneMat->baseShaderData(base.baseShaderData());
    cloneMat->ignoreXMLData(base.ignoreXMLData());

    // Could just direct copy the arrays, but this looks cool
    for (U8 s = 0u; s < to_U8(RenderStage::COUNT); ++s) {
        for (U8 p = 0u; p < to_U8(RenderPassType::COUNT); ++p) {
            for (U8 v = 0u; v < to_U8(RenderStagePass::VariantType::COUNT); ++v) {
                cloneMat->_shaderInfo[s][p][v] = _shaderInfo[s][p][v];
                cloneMat->_defaultRenderStates[s][p][v] = _defaultRenderStates[s][p][v];
            }
        }
    }

    for (U8 i = 0; i < to_U8(base._textures.size()); ++i) {
        const Texture_ptr& tex = base._textures[i];
        if (tex) {
            cloneMat->setTexture(static_cast<TextureUsage>(i), tex, base._samplers[i], base._textureOperations[i], base._useForPrePass[i]);
        }
    }
    cloneMat->_needsNewShader = true;

    return cloneMat;
}

bool Material::update([[maybe_unused]] const U64 deltaTimeUS) {

    if (_needsNewShader) {
        recomputeShaders();
        _needsNewShader = false;
        return true;
    }

    return false;
}

bool Material::setSampler(const TextureUsage textureUsageSlot, const size_t samplerHash, const bool applyToInstances)
{
    if (applyToInstances) {
        for (Material* instance : _instances) {
            instance->setSampler(textureUsageSlot, samplerHash, true);
        }
    }

    const U32 slot = to_U32(textureUsageSlot);
    if (_textureAddresses[slot] != s_defaultTextureAddress &&  _samplers[slot] != samplerHash) {
        assert(_textures[slot] != nullptr);
        assert(_textures[slot]->getState() == ResourceState::RES_LOADED);
        _textureAddresses[slot] = _textures[slot]->getGPUAddress(samplerHash);
    }

    _samplers[slot] = samplerHash;

    return true;
}

bool Material::setTexture(const TextureUsage textureUsageSlot, const Texture_ptr& texture, const size_t samplerHash, const TextureOperation op, const TexturePrePassUsage prePassUsage, const bool applyToInstances)
{
    if (applyToInstances) {
        for (Material* instance : _instances) {
            instance->setTexture(textureUsageSlot, texture, samplerHash, op, prePassUsage, applyToInstances);
        }
    }
    const U32 slot = to_U32(textureUsageSlot);

    if (samplerHash != _samplers[slot]) {
        setSampler(textureUsageSlot, samplerHash, applyToInstances);
    }

    setTextureOperation(textureUsageSlot, texture ? op : TextureOperation::NONE, false);

    {
        ScopedLock<SharedMutex> w_lock(_textureLock);
        if (_textures[slot]) {
            // Skip adding same texture
            if (texture != nullptr && _textures[slot]->getGUID() == texture->getGUID()) {
                return true;
            }
        }

        _useForPrePass[slot] = texture ? prePassUsage : TexturePrePassUsage::AUTO;

        _textures[slot] = texture;
        _textureAddresses[slot] = texture ? texture->getGPUAddress(samplerHash) : s_defaultTextureAddress;

        if (textureUsageSlot == TextureUsage::METALNESS) {
            _usePackedOMR = (texture != nullptr && texture->numChannels() > 2u);
        }
        
        if (textureUsageSlot == TextureUsage::UNIT0 ||
            textureUsageSlot == TextureUsage::OPACITY)
        {
            bool isOpacity = true;
            if (textureUsageSlot == TextureUsage::UNIT0) {
                _textureKeyCache = texture == nullptr ? std::numeric_limits<I32>::lowest() : (useBindlessTextures() ? 0u : texture->data()._textureHandle);
                isOpacity = false;
            }

            updateTransparency();
        }
    }

    _needsNewShader = true;

    return true;
}

void Material::setTextureOperation(const TextureUsage textureUsageSlot, 
                                   const TextureOperation op, 
                                   const bool applyToAllInstances) {

    TextureOperation& crtOp = _textureOperations[to_base(textureUsageSlot)];

    if (crtOp != op) {
        crtOp = op;
        _needsNewShader = true;
    }

    if (applyToAllInstances) {
        for (Material* instance : _instances) {
            instance->setTextureOperation(textureUsageSlot, op, true);
        }
    }
}

void Material::setShaderProgramInternal(const ShaderProgramDescriptor& shaderDescriptor,
                                        ShaderProgramInfo& shaderInfo,
                                        const RenderStagePass stagePass) const
{
    OPTICK_EVENT();

    ShaderProgramDescriptor shaderDescriptorRef = shaderDescriptor;
    computeAndAppendShaderDefines(shaderDescriptorRef, stagePass);

    ResourceDescriptor shaderResDescriptor(shaderDescriptorRef._name);
    shaderResDescriptor.propertyDescriptor(shaderDescriptorRef);
    shaderResDescriptor.threaded(false);

    ShaderProgram_ptr shader = CreateResource<ShaderProgram>(_context.parent().resourceCache(), shaderResDescriptor);
    if (shader != nullptr) {
        const ShaderProgram* oldShader = shaderInfo._shaderRef.get();
        if (oldShader != nullptr) {
            const char* newShaderName = shader == nullptr ? nullptr : shader->resourceName().c_str();

            if (newShaderName == nullptr || strlen(newShaderName) == 0 || oldShader->resourceName().compare(newShaderName) != 0) {
                // We cannot replace a shader that is still loading in the background
                WAIT_FOR_CONDITION(oldShader->getState() == ResourceState::RES_LOADED);
                    Console::printfn(Locale::Get(_ID("REPLACE_SHADER")),
                        oldShader->resourceName().c_str(),
                        newShaderName != nullptr ? newShaderName : "NULL",
                        TypeUtil::RenderStageToString(stagePass._stage),
                        TypeUtil::RenderPassTypeToString(stagePass._passType),
                        to_base(stagePass._variant));
            }
        }
    }

    shaderInfo._shaderRef = shader;
    shaderInfo._shaderCompStage = shader == nullptr || shader->getState() == ResourceState::RES_LOADED
                                                     ? (shaderInfo._customShader ? ShaderBuildStage::COMPUTED : ShaderBuildStage::READY)
                                                     : ShaderBuildStage::COMPUTED;

    if (shader != nullptr && shaderInfo._shaderCompStage == ShaderBuildStage::READY) {
        shaderInfo._shaderKeyCache = shaderInfo._shaderRef->getGUID();
    }
}

void Material::setShaderProgramInternal(const ShaderProgramDescriptor& shaderDescriptor,
                                        const RenderStagePass stagePass,
                                        const bool computeOnAdd)
{
    OPTICK_EVENT();

    ShaderProgramDescriptor shaderDescriptorRef = shaderDescriptor;
    computeAndAppendShaderDefines(shaderDescriptorRef, stagePass);

    ResourceDescriptor shaderResDescriptor{ shaderDescriptorRef._name };
    shaderResDescriptor.propertyDescriptor(shaderDescriptorRef);
    shaderResDescriptor.threaded(false);

    ShaderProgramInfo& info = shaderInfo(stagePass);
    // if we already have a different shader assigned ...
    if (info._shaderRef != nullptr && info._shaderRef->resourceName().compare(shaderDescriptorRef._name) != 0)
    {
        // We cannot replace a shader that is still loading in the background
        WAIT_FOR_CONDITION(info._shaderRef->getState() == ResourceState::RES_LOADED);
        Console::printfn(Locale::Get(_ID("REPLACE_SHADER")),
            info._shaderRef->resourceName().c_str(),
            shaderDescriptorRef._name.c_str(),
            TypeUtil::RenderStageToString(stagePass._stage),
            TypeUtil::RenderPassTypeToString(stagePass._passType),
            stagePass._variant);
    }

    ShaderComputeQueue::ShaderQueueElement shaderElement{ info._shaderRef, shaderDescriptorRef };
    if (computeOnAdd) {
        _context.shaderComputeQueue().process(shaderElement);
        info._shaderCompStage = ShaderBuildStage::COMPUTED;
        assert(info._shaderRef != nullptr);
    } else {
        _context.shaderComputeQueue().addToQueueBack(shaderElement);
        info._shaderCompStage = ShaderBuildStage::QUEUED;
    }
}

void Material::recomputeShaders() {
    OPTICK_EVENT();

    for (U8 s = 0u; s < to_U8(RenderStage::COUNT); ++s) {
        for (U8 p = 0u; p < to_U8(RenderPassType::COUNT); ++p) {
            RenderStagePass stagePass{ static_cast<RenderStage>(s), static_cast<RenderPassType>(p) };
            ShaderPerVariant& variantMap = _shaderInfo[s][p];

            for (U8 v = 0u; v < to_U8(RenderStagePass::VariantType::COUNT); ++v) {
                ShaderProgramInfo& shaderInfo = variantMap[v];
                if (shaderInfo._shaderCompStage == ShaderBuildStage::COUNT) {
                    continue;
                }

                stagePass._variant = static_cast<RenderStagePass::VariantType>(v);
                if (!shaderInfo._customShader) {
                    shaderInfo._shaderCompStage = ShaderBuildStage::REQUESTED;
                    computeShader(stagePass);
                } else {
                    if (shaderInfo._shaderCompStage == ShaderBuildStage::COMPUTED) {
                        shaderInfo._shaderCompStage = ShaderBuildStage::READY;
                    } else if (shaderInfo._shaderCompStage == ShaderBuildStage::READY && _customShaderCBK) {
                        _customShaderCBK(*this, stagePass);
                    }

                    if (shaderInfo._shaderRef != nullptr && shaderInfo._shaderCompStage == ShaderBuildStage::READY) {
                        shaderInfo._shaderKeyCache = shaderInfo._shaderRef->getGUID();
                    }
                }
            }
        }
    }
}

I64 Material::computeAndGetProgramGUID(const RenderStagePass renderStagePass) {
    constexpr U8 maxRetries = 250;

    bool justFinishedLoading = false;
    for (U8 i = 0; i < maxRetries; ++i) {
        if (!canDraw(renderStagePass, justFinishedLoading)) {
            if (!_context.shaderComputeQueue().stepQueue()) {
                NOP();
            }
        } else {
            return getProgramGUID(renderStagePass);
        }
    }

    return ShaderProgram::DefaultShader()->getGUID();
}

I64 Material::getProgramGUID(const RenderStagePass renderStagePass) const {

    const ShaderProgramInfo& info = shaderInfo(renderStagePass);

    if (info._shaderRef != nullptr) {
        WAIT_FOR_CONDITION(info._shaderRef->getState() == ResourceState::RES_LOADED);
        return info._shaderRef->getGUID();
    }
    DIVIDE_UNEXPECTED_CALL();

    return ShaderProgram::DefaultShader()->getGUID();
}

bool Material::canDraw(const RenderStagePass renderStagePass, bool& shaderJustFinishedLoading) {
    OPTICK_EVENT();

    shaderJustFinishedLoading = false;
    ShaderProgramInfo& info = shaderInfo(renderStagePass);
    // If we have a shader queued (with a valid ref) ...
    if (info._shaderCompStage == ShaderBuildStage::QUEUED) {
        // ... we are now passed the "compute" stage. We just need to wait for it to load
        if (info._shaderRef != nullptr) {
            info._shaderCompStage = ShaderBuildStage::COMPUTED;
        } else {
            // Shader is still in the queue
            return false;
        }
    }

    // If the shader is computed ...
    if (info._shaderCompStage == ShaderBuildStage::COMPUTED) {
        assert(info._shaderRef != nullptr);

        // ... wait for the shader to finish loading
        if (info._shaderRef->getState() != ResourceState::RES_LOADED) {
            return false;
        }
        // Once it has finished loading, it is ready for drawing
        shaderJustFinishedLoading = true;
        info._shaderCompStage = ShaderBuildStage::READY;
        info._shaderKeyCache = info._shaderRef->getGUID();
    }

    // If the shader isn't ready it may have not passed through the computational stage yet (e.g. the first time this method is called)
    if (info._shaderCompStage != ShaderBuildStage::READY) {
        // If it's a custom shader, not much we can do as custom shaders are either already computed or ready
        if (info._customShader) {
            // Just wait for it to load
            DIVIDE_ASSERT(info._shaderCompStage == ShaderBuildStage::COMPUTED);
            return false;
        }

        // This is usually the first step in generating a shader: No shader available but we need to render in this stagePass
        if (info._shaderCompStage == ShaderBuildStage::COUNT) {
            // So request a new shader
            info._shaderCompStage = ShaderBuildStage::REQUESTED;
            computeShader(renderStagePass);
        }

        return false;
    }

    // Shader should be in the ready state
    return true;
}
void Material::computeAndAppendShaderDefines(ShaderProgramDescriptor& shaderDescriptor, const RenderStagePass renderStagePass) const {
    OPTICK_EVENT();

    const bool isDepthPass = IsDepthPass(renderStagePass);

    DIVIDE_ASSERT(_shadingMode != ShadingMode::COUNT, "Material computeShader error: Invalid shading mode specified!");

    std::array<ModuleDefines, to_base(ShaderType::COUNT)> moduleDefines = {};

    ModuleDefines globalDefines = {};

    if (renderStagePass._stage == RenderStage::SHADOW) {
        globalDefines.emplace_back("SHADOW_PASS", true);
        if (to_U8(renderStagePass._variant) == to_U8(LightType::DIRECTIONAL)) {
            moduleDefines[to_base(ShaderType::FRAGMENT)].emplace_back("ORTHO_PROJECTION", true);
        }
    } else if (isDepthPass) {
        globalDefines.emplace_back("PRE_PASS", true);
    }
    if (renderStagePass._stage == RenderStage::REFLECTION && to_U8(renderStagePass._variant) != to_base(ReflectorType::CUBE)) {
        globalDefines.emplace_back("REFLECTION_PASS", true);
    }
    if (renderStagePass._stage == RenderStage::DISPLAY) {
        globalDefines.emplace_back("MAIN_DISPLAY_PASS", true);
    }
    if (renderStagePass._passType == RenderPassType::OIT_PASS) {
        moduleDefines[to_base(ShaderType::FRAGMENT)].emplace_back("OIT_PASS", true);
    }

    // Display pre-pass caches normal maps in a GBuffer, so it's the only exception
    if ((!isDepthPass || renderStagePass._stage == RenderStage::DISPLAY) &&
        _textures[to_base(TextureUsage::NORMALMAP)] != nullptr &&
        _bumpMethod != BumpMethod::NONE) 
    {
        // Bump mapping?
        globalDefines.emplace_back("COMPUTE_TBN", true);
    }

    if (hasTransparency()) {
        moduleDefines[to_base(ShaderType::FRAGMENT)].emplace_back("HAS_TRANSPARENCY", true);

        if (useAlphaDiscard() && renderStagePass._passType != RenderPassType::OIT_PASS) {
            moduleDefines[to_base(ShaderType::FRAGMENT)].emplace_back("USE_ALPHA_DISCARD", true);
        }
    }

    const Configuration& config = _parentCache->context().config();
    if (!config.rendering.shadowMapping.enabled) {
        moduleDefines[to_base(ShaderType::FRAGMENT)].emplace_back("DISABLE_SHADOW_MAPPING", true);
    } else {
        if (!config.rendering.shadowMapping.csm.enabled) {
            moduleDefines[to_base(ShaderType::FRAGMENT)].emplace_back("DISABLE_SHADOW_MAPPING_CSM", true);
        }
        if (!config.rendering.shadowMapping.spot.enabled) {
            moduleDefines[to_base(ShaderType::FRAGMENT)].emplace_back("DISABLE_SHADOW_MAPPING_SPOT", true);
        }
        if (!config.rendering.shadowMapping.point.enabled) {
            moduleDefines[to_base(ShaderType::FRAGMENT)].emplace_back("DISABLE_SHADOW_MAPPING_POINT", true);
        }
    }

    globalDefines.emplace_back(_isStatic ? "NODE_STATIC" : "NODE_DYNAMIC", true);

    if (_hardwareSkinning) {
        moduleDefines[to_base(ShaderType::VERTEX)].emplace_back("USE_GPU_SKINNING", true);
    }

    for (ShaderModuleDescriptor& module : shaderDescriptor._modules) {
        module._defines.insert(eastl::end(module._defines), eastl::begin(globalDefines), eastl::end(globalDefines));
        module._defines.insert(eastl::end(module._defines), eastl::begin(moduleDefines[to_base(module._moduleType)]), eastl::end(moduleDefines[to_base(module._moduleType)]));
        module._defines.insert(eastl::end(module._defines), eastl::begin(_extraShaderDefines[to_base(module._moduleType)]), eastl::end(_extraShaderDefines[to_base(module._moduleType)]));
        module._defines.emplace_back("DEFINE_PLACEHOLDER", false);
        shaderDescriptor._name.append(Util::StringFormat("_%zu", ShaderProgram::DefinesHash(module._defines)));
    }
}

/// If the current material doesn't have a shader associated with it, then add the default ones.
void Material::computeShader(const RenderStagePass renderStagePass) {
    OPTICK_EVENT();

    const bool isDepthPass = IsDepthPass(renderStagePass);
    const bool isZPrePass = isDepthPass && renderStagePass._stage == RenderStage::DISPLAY;
    const bool isShadowPass = renderStagePass._stage == RenderStage::SHADOW;

    const Str64 vertSource = isDepthPass ? baseShaderData()._depthShaderVertSource : baseShaderData()._colourShaderVertSource;
    const Str64 fragSource = isDepthPass ? baseShaderData()._depthShaderFragSource : baseShaderData()._colourShaderFragSource;

    Str32 vertVariant = isDepthPass 
                            ? isShadowPass 
                                ? baseShaderData()._shadowShaderVertVariant
                                : baseShaderData()._depthShaderVertVariant
                            : baseShaderData()._colourShaderVertVariant;
    Str32 fragVariant = isDepthPass ? baseShaderData()._depthShaderFragVariant : baseShaderData()._colourShaderFragVariant;
    ShaderProgramDescriptor shaderDescriptor{};
    shaderDescriptor._name = vertSource + "_" + fragSource;

    if (isShadowPass) {
        vertVariant += "Shadow";
        fragVariant += "Shadow.VSM";
        if (to_U8(renderStagePass._variant) == to_U8(LightType::DIRECTIONAL)) {
            fragVariant += ".ORTHO";
        }
    } else if (isDepthPass) {
        vertVariant += "PrePass";
        fragVariant += "PrePass";
    }

    ShaderModuleDescriptor vertModule = {};
    vertModule._variant = vertVariant;
    vertModule._sourceFile = (vertSource + ".glsl").c_str();
    vertModule._batchSameFile = false;
    vertModule._moduleType = ShaderType::VERTEX;
    shaderDescriptor._modules.push_back(vertModule);

    if (!isDepthPass || isZPrePass) {
        ShaderModuleDescriptor fragModule = {};
        fragModule._variant = fragVariant;
        fragModule._sourceFile = (fragSource + ".glsl").c_str();
        fragModule._moduleType = ShaderType::FRAGMENT;

        shaderDescriptor._modules.push_back(fragModule);
    }

    setShaderProgramInternal(shaderDescriptor, renderStagePass, false);
}

bool Material::unload() {
    _textures.fill(nullptr);

    static ShaderProgramInfo defaultShaderInfo = {};

    for (U8 s = 0u; s < to_U8(RenderStage::COUNT); ++s) {
        ShaderVariantsPerPass& passMapShaders = _shaderInfo[s];
        StateVariantsPerPass& passMapStates = _defaultRenderStates[s];
        for (U8 p = 0u; p < to_U8(RenderPassType::COUNT); ++p) {
            passMapShaders[p].fill(defaultShaderInfo);
            passMapStates[p].fill(g_invalidStateHash);
        }
    }
    
    if (_baseMaterial != nullptr) {
        erase_if(_baseMaterial->_instances,
                 [guid = getGUID()](Material* instance) noexcept {
                     return instance->getGUID() == guid;
                 });
    }

    for (Material* instance : _instances) {
        instance->_baseMaterial = nullptr;
    }

    return true;
}

void Material::refractive(const bool state, const bool applyToInstances) {
    if (_isRefractive != state) {
        _isRefractive = state;
        _needsNewShader = true;
    }

    if (applyToInstances) {
        for (Material* instance : _instances) {
            instance->refractive(state, true);
        }
    }
}

void Material::doubleSided(const bool state, const bool applyToInstances) {
    if (_doubleSided != state) {
        _doubleSided = state;
        _needsNewShader = true;
    }

    if (applyToInstances) {
        for (Material* instance : _instances) {
            instance->doubleSided(state, true);
        }
    }
}

void Material::receivesShadows(const bool state, const bool applyToInstances) {
    if (_receivesShadows != state) {
        _receivesShadows = state;
        _needsNewShader = true;
    }

    if (applyToInstances) {
        for (Material* instance : _instances) {
            instance->receivesShadows(state, true);
        }
    }
}

void Material::isStatic(const bool state, const bool applyToInstances) {
    if (_isStatic != state) {
        _isStatic = state;
        _needsNewShader = true;
    }

    if (applyToInstances) {
        for (Material* instance : _instances) {
            instance->isStatic(state, true);
        }
    }
}

void Material::ignoreTexDiffuseAlpha(const bool state, const bool applyToInstances) {
    if (_ignoreTexDiffuseAlpha != state) {
        _ignoreTexDiffuseAlpha = state;
        _needsNewShader = true;
    }

    if (applyToInstances) {
        for (Material* instance : _instances) {
            instance->ignoreTexDiffuseAlpha(state, true);
        }
    }
}

void Material::hardwareSkinning(const bool state, const bool applyToInstances) {
    if (_hardwareSkinning != state) {
        _hardwareSkinning = state;
        _needsNewShader = true;
    }

    if (applyToInstances) {
        for (Material* instance : _instances) {
            instance->hardwareSkinning(state, true);
        }
    }
}

void Material::setShaderProgram(const ShaderProgramDescriptor& shaderDescriptor, const RenderStage stage, const RenderPassType pass, const RenderStagePass::VariantType variant) {
    RenderStagePass stagePass;
    for (U8 s = 0u; s < to_U8(RenderStage::COUNT); ++s) {
        for (U8 p = 0u; p < to_U8(RenderPassType::COUNT); ++p) {
            stagePass._stage = static_cast<RenderStage>(s);
            stagePass._passType = static_cast<RenderPassType>(p);
            if ((stage == RenderStage::COUNT || stage == stagePass._stage) && (pass == RenderPassType::COUNT || pass == stagePass._passType)) {
                if (variant == RenderStagePass::VariantType::COUNT) {
                    for (U8 i = 0u; i < to_base(RenderStagePass::VariantType::COUNT); ++i) {
                        ShaderProgramInfo& shaderInfo = _shaderInfo[s][p][i];
                        shaderInfo._customShader = true;
                        shaderInfo._shaderCompStage = ShaderBuildStage::COUNT;
                        stagePass._variant = static_cast<RenderStagePass::VariantType>(variant);
                        setShaderProgramInternal(shaderDescriptor, shaderInfo, stagePass);
                    }
                } else {
                    stagePass._variant = variant;
                    ShaderProgramInfo& shaderInfo = _shaderInfo[s][p][to_base(variant)];
                    shaderInfo._customShader = true;
                    shaderInfo._shaderCompStage = ShaderBuildStage::COUNT;
                    setShaderProgramInternal(shaderDescriptor, shaderInfo, stagePass);
                }
            }
        }
    }
}

void Material::setRenderStateBlock(const size_t renderStateBlockHash, const RenderStage stage, const RenderPassType pass, const RenderStagePass::VariantType variant) {
    for (U8 s = 0u; s < to_U8(RenderStage::COUNT); ++s) {
        for (U8 p = 0u; p < to_U8(RenderPassType::COUNT); ++p) {
            const RenderStage crtStage = static_cast<RenderStage>(s);
            const RenderPassType crtPass = static_cast<RenderPassType>(p);
            if ((stage == RenderStage::COUNT || stage == crtStage) && (pass == RenderPassType::COUNT || pass == crtPass)) {
                if (variant == RenderStagePass::VariantType::COUNT) {
                    _defaultRenderStates[s][p].fill(renderStateBlockHash);
                } else {
                    _defaultRenderStates[s][p][to_base(variant)] = renderStateBlockHash;
                }
            }
        }
    }
}

void Material::toggleTransparency(const bool state, const bool applyToInstances) {
    if (_transparencyEnabled != state) {
        _transparencyEnabled = state;
        _needsNewShader = true;
    }

    if (applyToInstances) {
        for (Material* instance : _instances) {
            instance->toggleTransparency(state, true);
        }
    }
}

void Material::useAlphaDiscard(const bool state, const bool applyToInstances) {
    if (_useAlphaDiscard != state) {
        _useAlphaDiscard = state;
        _needsNewShader = true;
    }

    if (applyToInstances) {
        for (Material* instance : _instances) {
            instance->useAlphaDiscard(state, true);
        }
    }
}

void Material::baseColour(const FColour4& colour, const bool applyToInstances) {
    _baseColour = colour;
    updateTransparency();

    if (applyToInstances) {
        for (Material* instance : _instances) {
            instance->baseColour(colour, true);
        }
    }
}

void Material::emissiveColour(const FColour3& colour, const bool applyToInstances) {
    _emissive = colour;

    if (applyToInstances) {
        for (Material* instance : _instances) {
            instance->emissiveColour(colour, true);
        }
    }
}

void Material::ambientColour(const FColour3& colour, const bool applyToInstances) {
    _ambient = colour;

    if (applyToInstances) {
        for (Material* instance : _instances) {
            instance->ambientColour(colour, true);
        }
    }
}

void Material::metallic(const F32 value, const bool applyToInstances) {
    _metallic = CLAMPED_01(value);

    if (applyToInstances) {
        for (Material* instance : _instances) {
            instance->metallic(value, true);
        }
    }
}

void Material::roughness(const F32 value, const bool applyToInstances) {
    _roughness = CLAMPED_01(value);

    if (applyToInstances) {
        for (Material* instance : _instances) {
            instance->roughness(value, true);
        }
    }
}

void Material::occlusion(const F32 value, const bool applyToInstances) {
    _occlusion = CLAMPED_01(value);

    if (applyToInstances) {
        for (Material* instance : _instances) {
            instance->occlusion(value, true);
        }
    }
}

void Material::specularColour(const FColour3& value, const bool applyToInstances) {
    _specular.rgb = value;

    if (applyToInstances) {
        for (Material* instance : _instances) {
            instance->specularColour(value, true);
        }
    }
}

void Material::specGloss(const SpecularGlossiness& value, const bool applyToInstances) {
    _specGloss = value;

    if (applyToInstances) {
        for (Material* instance : _instances) {
            instance->specGloss(value, true);
        }
    }
}

void Material::shininess(const F32 value, const bool applyToInstances) {
    _specular.a = value;

    if (applyToInstances) {
        for (Material* instance : _instances) {
            instance->shininess(value, true);
        }
    }
}

void Material::parallaxFactor(const F32 factor, const bool applyToInstances) {
    _parallaxFactor = CLAMPED_01(factor);

    if (applyToInstances) {
        for (Material* instance : _instances) {
            instance->parallaxFactor(factor, true);
        }
    }
}

void Material::bumpMethod(const BumpMethod newBumpMethod, const bool applyToInstances) {
    if (_bumpMethod != newBumpMethod) {
        _bumpMethod = newBumpMethod;
        _needsNewShader = true;
    }

    if (applyToInstances) {
        for (Material* instance : _instances) {
            instance->bumpMethod(newBumpMethod, true);
        }
    }
}

void Material::shadingMode(const ShadingMode mode, const bool applyToInstances) {
    if (_shadingMode != mode) {
        _shadingMode = mode;
    }

    if (applyToInstances) {
        for (Material* instance : _instances) {
            instance->shadingMode(mode, true);
        }
    }
}

void Material::updateTransparency() {
    if (!_transparencyEnabled) {
        return;
    }

    const TranslucencySource oldSource = _translucencySource;
    _translucencySource = TranslucencySource::COUNT;

    // In order of importance (less to more)!
    // diffuse channel alpha
    if (baseColour().a < 0.95f && _textureOperations[to_base(TextureUsage::UNIT0)] != TextureOperation::REPLACE) {
        _translucencySource = TranslucencySource::ALBEDO_COLOUR;
    }

    // base texture is translucent
    const Texture_ptr& albedo = _textures[to_base(TextureUsage::UNIT0)];
    if (albedo && albedo->hasTransparency() && !_ignoreTexDiffuseAlpha) {
        _translucencySource = TranslucencySource::ALBEDO_TEX;
    }

    // opacity map
    const Texture_ptr& opacity = _textures[to_base(TextureUsage::OPACITY)];
    if (opacity) {
        const U8 channelCount = NumChannels(opacity->descriptor().baseFormat());
        _translucencySource = (channelCount == 4 && opacity->hasTransparency()) ? TranslucencySource::OPACITY_MAP_A : TranslucencySource::OPACITY_MAP_R;
    }

    _needsNewShader = oldSource != _translucencySource;
}

size_t Material::getRenderStateBlock(const RenderStagePass renderStagePass) const {
    const StatesPerVariant& variantMap = _defaultRenderStates[to_base(renderStagePass._stage)][to_base(renderStagePass._passType)];

    size_t ret = variantMap[to_base(renderStagePass._variant)];
    // If we haven't defined a state for this variant, use the default one
    if (ret == g_invalidStateHash) {
        ret = variantMap[0u];
    }

    if (ret != g_invalidStateHash) {
        // We don't need to update cull params for shadow mapping unless this is a directional light
        // since CSM splits cause all sorts of errors
        if (_doubleSided) {
            RenderStateBlock tempBlock = RenderStateBlock::get(ret);
            tempBlock.setCullMode(CullMode::NONE);
            ret = tempBlock.getHash();
        }
    }

    return ret;
}

void Material::getSortKeys(const RenderStagePass renderStagePass, I64& shaderKey, I32& textureKey) const {
    shaderKey = shaderInfo(renderStagePass)._shaderKeyCache;
    textureKey = _textureKeyCache;
}

FColour4 Material::getBaseColour(bool& hasTextureOverride, Texture*& textureOut) const noexcept {
    textureOut = nullptr;
    hasTextureOverride = _textures[to_base(TextureUsage::UNIT0)] != nullptr;
    if (hasTextureOverride) {
        textureOut = _textures[to_base(TextureUsage::UNIT0)].get();
    }
    return baseColour();
}

FColour3 Material::getEmissive(bool& hasTextureOverride, Texture*& textureOut) const noexcept {
    textureOut = nullptr;
    hasTextureOverride = _textures[to_base(TextureUsage::EMISSIVE)] != nullptr;
    if (hasTextureOverride) {
        textureOut = _textures[to_base(TextureUsage::EMISSIVE)].get();
    }

    return emissive();
}

FColour3 Material::getAmbient(bool& hasTextureOverride, Texture*& textureOut) const noexcept {
    textureOut = nullptr;
    hasTextureOverride = false;

    return ambient();
}

FColour4 Material::getSpecular(bool& hasTextureOverride, Texture*& textureOut) const noexcept {
    textureOut = nullptr;
    hasTextureOverride = _textures[to_base(TextureUsage::SPECULAR)] != nullptr;
    if (hasTextureOverride) {
        textureOut = _textures[to_base(TextureUsage::SPECULAR)].get();
    }
    return specular();
}

F32 Material::getMetallic(bool& hasTextureOverride, Texture*& textureOut) const noexcept {
    textureOut = nullptr;
    hasTextureOverride = _textures[to_base(TextureUsage::METALNESS)] != nullptr;
    if (hasTextureOverride) {
        textureOut = _textures[to_base(TextureUsage::METALNESS)].get();
    }
    return metallic();
}

F32 Material::getRoughness(bool& hasTextureOverride, Texture*& textureOut) const noexcept {
    textureOut = nullptr;
    hasTextureOverride = _textures[to_base(TextureUsage::ROUGHNESS)] != nullptr;
    if (hasTextureOverride) {
        textureOut = _textures[to_base(TextureUsage::ROUGHNESS)].get();
    }
    return roughness();
}

F32 Material::getOcclusion(bool& hasTextureOverride, Texture*& textureOut) const noexcept {
    textureOut = nullptr;
    hasTextureOverride = _textures[to_base(TextureUsage::OCCLUSION)] != nullptr;
    if (hasTextureOverride) {
        textureOut = _textures[to_base(TextureUsage::OCCLUSION)].get();
    }
    return occlusion();
}

void Material::getData(const RenderingComponent& parentComp, const U32 bestProbeID, NodeMaterialData& dataOut) {
    const SceneGraphNode* parentSGN = parentComp.parentSGN();

    F32 selectionFlag = 0.0f;
    // We don't propagate selection flags to children outside of the editor, so check for that
    if (parentSGN->hasFlag(SceneGraphNode::Flags::SELECTED) ||
        parentSGN->parent() && parentSGN->parent()->hasFlag(SceneGraphNode::Flags::SELECTED)) {
        selectionFlag = 1.0f;
    } else if (parentSGN->hasFlag(SceneGraphNode::Flags::HOVERED)) {
        selectionFlag = 0.5f;
    }

    const FColour4& specColour = specular(); //< For PHONG_SPECULAR

    const bool useOpacityAlphaChannel = _translucencySource == TranslucencySource::OPACITY_MAP_A;
    const bool useAlbedoTexAlphachannel = _translucencySource == TranslucencySource::ALBEDO_TEX;

    //ToDo: Maybe store all of these material properties in an internal, cached, NodeMaterialData structure? -Ionut
    dataOut._albedo.set(baseColour());
    dataOut._colourData.set(ambient(), specColour.a);
    dataOut._emissiveAndParallax.set(emissive(), parallaxFactor());
    dataOut._data.x = Util::PACK_UNORM4x8(occlusion(), metallic(), roughness(), selectionFlag);
    dataOut._data.y = Util::PACK_UNORM4x8(specColour.r, specColour.g, specColour.b, (doubleSided() ? 1.f : 0.f));
    dataOut._data.z = Util::PACK_UNORM4x8(0u, to_U8(shadingMode()), usePackedOMR() ? 1u : 0u, to_U8(bumpMethod()));
    dataOut._data.w = bestProbeID;

    dataOut._textureOperations.x = Util::PACK_UNORM4x8(to_U8(_textureOperations[to_base(TextureUsage::UNIT0)]),
                                                       to_U8(_textureOperations[to_base(TextureUsage::UNIT1)]),
                                                       to_U8(_textureOperations[to_base(TextureUsage::SPECULAR)]),
                                                       to_U8(_textureOperations[to_base(TextureUsage::EMISSIVE)]));
    dataOut._textureOperations.y = Util::PACK_UNORM4x8(to_U8(_textureOperations[to_base(TextureUsage::OCCLUSION)]),
                                                       to_U8(_textureOperations[to_base(TextureUsage::METALNESS)]),
                                                       to_U8(_textureOperations[to_base(TextureUsage::ROUGHNESS)]),
                                                       to_U8(_textureOperations[to_base(TextureUsage::OPACITY)]));
    dataOut._textureOperations.z = Util::PACK_UNORM4x8(useAlbedoTexAlphachannel ? 1.f : 0.f, 
                                                       useOpacityAlphaChannel ? 1.f : 0.f,
                                                       specGloss().x,
                                                       specGloss().y);
    dataOut._textureOperations.w = Util::PACK_UNORM4x8(receivesShadows() ? 1.f : 0.f, 0.f, 0.f, 0.f);
}

void Material::getTextures(const RenderingComponent& parentComp, NodeMaterialTextures& texturesOut) {
    for (U8 i = 0u; i < MATERIAL_TEXTURE_COUNT; ++i) {
        texturesOut[i] = TextureToUVec2(_textureAddresses[to_base(g_materialTextures[i])]);
    }
}

bool Material::getTextureData(const RenderStagePass renderStagePass, TextureDataContainer& textureData) {
    OPTICK_EVENT();
    // We only need to actually bind NON-RESIDENT textures. 
    if (useBindlessTextures() && !debugBindlessTextures()) {
        return true;
    }

    const bool isPrePass = (renderStagePass._passType == RenderPassType::PRE_PASS);
    const auto addTexture = [&](const U8 slot) {
        const Texture_ptr& crtTexture = _textures[slot];
        if (crtTexture != nullptr) {
            textureData.add(TextureEntry{ crtTexture->data(), _samplers[slot], slot });
            return true;
        }
        return false;
    };

    SharedLock<SharedMutex> r_lock(_textureLock);
    bool ret = false;
    if (!isPrePass) {
        for (const U8 slot : g_materialTextureSlots) {
            if (addTexture(slot)) {
                ret = true;
            }
        }
    } else {
        for (U8 i = 0u; i < to_U8(TextureUsage::COUNT); ++i) {
            bool add = false;
            switch (_useForPrePass[i]) {
                case TexturePrePassUsage::ALWAYS: {
                    // Always add
                    add = true;
                } break;
                case TexturePrePassUsage::NEVER: {
                    //Skip
                    add = false;
                } break;
                case TexturePrePassUsage::AUTO: {
                    // Some best-fit heuristics that will surely break at one point
                    switch (static_cast<TextureUsage>(i)) {
                        case TextureUsage::UNIT0: {
                            add = hasTransparency() && _translucencySource == TranslucencySource::ALBEDO_TEX;
                        } break;
                        case TextureUsage::NORMALMAP: {
                            add = renderStagePass._stage == RenderStage::DISPLAY;
                        } break;
                        case TextureUsage::SPECULAR: {
                            add = renderStagePass._stage == RenderStage::DISPLAY && 
                                  (shadingMode() != ShadingMode::PBR_MR && shadingMode() != ShadingMode::PBR_SG);
                        } break;
                        case TextureUsage::METALNESS: {
                            add = renderStagePass._stage == RenderStage::DISPLAY && _usePackedOMR;
                        } break;
                        case TextureUsage::ROUGHNESS: {
                            add = renderStagePass._stage == RenderStage::DISPLAY && !_usePackedOMR;
                        } break;
                        case TextureUsage::HEIGHTMAP: {
                            add = true;
                        } break;
                        case TextureUsage::OPACITY: {
                            add = hasTransparency() &&
                                (_translucencySource == TranslucencySource::OPACITY_MAP_A || _translucencySource == TranslucencySource::OPACITY_MAP_R);
                        } break;
                    };
                } break;


            }

            if (add && addTexture(i)) {
                ret = true;
            }
        }
    }

    return ret;
}

void Material::rebuild() {
    recomputeShaders();

    // Alternatively we could just copy the maps directly
    for (U8 s = 0u; s < to_U8(RenderStage::COUNT); ++s) {
        for (U8 p = 0u; p < to_U8(RenderPassType::COUNT); ++p) {
            ShaderPerVariant& shaders = _shaderInfo[s][p];
            for (const ShaderProgramInfo& info : shaders) {
                if (info._shaderRef != nullptr && info._shaderRef->getState() == ResourceState::RES_LOADED) {
                    info._shaderRef->recompile();
                }
            }
        }
    }
}

void Material::saveToXML(const string& entryName, boost::property_tree::ptree& pt) const {
    pt.put(entryName + ".version", g_materialXMLVersion);

    pt.put(entryName + ".shadingMode", TypeUtil::ShadingModeToString(shadingMode()));

    pt.put(entryName + ".colour.<xmlattr>.r", baseColour().r);
    pt.put(entryName + ".colour.<xmlattr>.g", baseColour().g);
    pt.put(entryName + ".colour.<xmlattr>.b", baseColour().b);
    pt.put(entryName + ".colour.<xmlattr>.a", baseColour().a);

    pt.put(entryName + ".emissive.<xmlattr>.r", emissive().r);
    pt.put(entryName + ".emissive.<xmlattr>.g", emissive().g);
    pt.put(entryName + ".emissive.<xmlattr>.b", emissive().b);

    pt.put(entryName + ".ambient.<xmlattr>.r", ambient().r);
    pt.put(entryName + ".ambient.<xmlattr>.g", ambient().g);
    pt.put(entryName + ".ambient.<xmlattr>.b", ambient().b);

    pt.put(entryName + ".specular.<xmlattr>.r", specular().r);
    pt.put(entryName + ".specular.<xmlattr>.g", specular().g);
    pt.put(entryName + ".specular.<xmlattr>.b", specular().b);
    pt.put(entryName + ".specular.<xmlattr>.a", specular().a);

    pt.put(entryName + ".specular_factor", specGloss().x);
    pt.put(entryName + ".glossiness_factor", specGloss().y);

    pt.put(entryName + ".metallic", metallic());

    pt.put(entryName + ".roughness", roughness());

    pt.put(entryName + ".doubleSided", doubleSided());

    pt.put(entryName + ".receivesShadows", receivesShadows());

    pt.put(entryName + ".ignoreTexDiffuseAlpha", ignoreTexDiffuseAlpha());

    pt.put(entryName + ".bumpMethod", TypeUtil::BumpMethodToString(bumpMethod()));

    pt.put(entryName + ".parallaxFactor", parallaxFactor());

    pt.put(entryName + ".transparencyEnabled", transparencyEnabled());

    pt.put(entryName + ".useAlphaDiscard", useAlphaDiscard());

    pt.put(entryName + ".isRefractive", _isRefractive);

    saveRenderStatesToXML(entryName, pt);
    saveTextureDataToXML(entryName, pt);
}

void Material::loadFromXML(const string& entryName, const boost::property_tree::ptree& pt) {
    if (ignoreXMLData()) {
        return;
    }

    const size_t detectedVersion = pt.get<size_t>(entryName + ".version", 0);
    if (detectedVersion != g_materialXMLVersion) {
        Console::printfn(Locale::Get(_ID("MATERIAL_WRONG_VERSION")), assetName().c_str(), detectedVersion, g_materialXMLVersion);
        return;
    }

    shadingMode(TypeUtil::StringToShadingMode(pt.get<string>(entryName + ".shadingMode", TypeUtil::ShadingModeToString(shadingMode()))), false);

    baseColour(FColour4(pt.get<F32>(entryName + ".colour.<xmlattr>.r", baseColour().r),
                        pt.get<F32>(entryName + ".colour.<xmlattr>.g", baseColour().g),
                        pt.get<F32>(entryName + ".colour.<xmlattr>.b", baseColour().b),
                        pt.get<F32>(entryName + ".colour.<xmlattr>.a", baseColour().a)), false);
                          
    emissiveColour(FColour3(pt.get<F32>(entryName + ".emissive.<xmlattr>.r", emissive().r),
                            pt.get<F32>(entryName + ".emissive.<xmlattr>.g", emissive().g),
                            pt.get<F32>(entryName + ".emissive.<xmlattr>.b", emissive().b)), false);  
    
    ambientColour(FColour3(pt.get<F32>(entryName + ".ambient.<xmlattr>.r", ambient().r),
                           pt.get<F32>(entryName + ".ambient.<xmlattr>.g", ambient().g),
                           pt.get<F32>(entryName + ".ambient.<xmlattr>.b", ambient().b)), false);

    specularColour(FColour4(pt.get<F32>(entryName + ".specular.<xmlattr>.r", specular().r),
                            pt.get<F32>(entryName + ".specular.<xmlattr>.g", specular().g),
                            pt.get<F32>(entryName + ".specular.<xmlattr>.b", specular().b),
                            pt.get<F32>(entryName + ".specular.<xmlattr>.a", specular().a)), false);

    specGloss(SpecularGlossiness(pt.get<F32>(entryName + ".specular_factor", specGloss().x),
                                 pt.get<F32>(entryName + ".glossiness_factor", specGloss().y)));

    metallic(pt.get<F32>(entryName + ".metallic", metallic()), false);

    roughness(pt.get<F32>(entryName + ".roughness", roughness()), false);

    doubleSided(pt.get<bool>(entryName + ".doubleSided", doubleSided()), false);

    receivesShadows(pt.get<bool>(entryName + ".receivesShadows", receivesShadows()), false);

    ignoreTexDiffuseAlpha(pt.get<bool>(entryName + ".ignoreTexDiffuseAlpha", ignoreTexDiffuseAlpha()), false);

    bumpMethod(TypeUtil::StringToBumpMethod(pt.get<string>(entryName + ".bumpMethod", TypeUtil::BumpMethodToString(bumpMethod()))), false);

    parallaxFactor(pt.get<F32>(entryName + ".parallaxFactor", parallaxFactor()), false);

    toggleTransparency(pt.get<bool>(entryName + ".transparencyEnabled", transparencyEnabled()), true);
    
    useAlphaDiscard(pt.get<bool>(entryName + ".useAlphaDiscard", useAlphaDiscard()), true);

    refractive(pt.get<bool>(entryName + ".isRefractive", refractive()), false);

    loadRenderStatesFromXML(entryName, pt);
    loadTextureDataFromXML(entryName, pt);
}

void Material::saveRenderStatesToXML(const string& entryName, boost::property_tree::ptree& pt) const {
    hashMap<size_t, U32> previousHashValues;

    U32 blockIndex = 0u;
    for (U8 s = 0u; s < to_U8(RenderStage::COUNT); ++s) {
        for (U8 p = 0u; p < to_U8(RenderPassType::COUNT); ++p) {
            for (U8 v = 0u; v < to_U8(RenderStagePass::VariantType::COUNT); ++v) {
                // we could just use _defaultRenderStates[s][p][v] for a direct lookup, but this handles the odd double-sided / no cull case
                const size_t stateHash = getRenderStateBlock(
                    RenderStagePass{
                        static_cast<RenderStage>(s),
                        static_cast<RenderPassType>(p),
                        0u,
                        static_cast<RenderStagePass::VariantType>(v)
                    }
                );
                if (stateHash != g_invalidStateHash && previousHashValues.find(stateHash) == std::cend(previousHashValues)) {
                    blockIndex++;
                    RenderStateBlock::saveToXML(
                        RenderStateBlock::get(stateHash),
                        Util::StringFormat("%s.RenderStates.%u",
                                           entryName.c_str(),
                                           blockIndex),
                        pt);
                    previousHashValues[stateHash] = blockIndex;
                }
                pt.put(Util::StringFormat("%s.%s.%s.%d.id",
                            entryName.c_str(),
                            TypeUtil::RenderStageToString(static_cast<RenderStage>(s)),
                            TypeUtil::RenderPassTypeToString(static_cast<RenderPassType>(p)),
                            v), 
                    previousHashValues[stateHash]);
            }
        }
    }
}

void Material::loadRenderStatesFromXML(const string& entryName, const boost::property_tree::ptree& pt) {
    hashMap<U32, size_t> previousHashValues;

    for (U8 s = 0u; s < to_U8(RenderStage::COUNT); ++s) {
        for (U8 p = 0u; p < to_U8(RenderPassType::COUNT); ++p) {
            for (U8 v = 0u; v < to_U8(RenderStagePass::VariantType::COUNT); ++v) {
                const U32 stateIndex = pt.get<U32>(
                    Util::StringFormat("%s.%s.%s.%d.id", 
                            entryName.c_str(),
                            TypeUtil::RenderStageToString(static_cast<RenderStage>(s)),
                            TypeUtil::RenderPassTypeToString(static_cast<RenderPassType>(p)),
                            v
                        ),
                    0
                );
                if (stateIndex != 0) {
                    const auto& it = previousHashValues.find(stateIndex);
                    if (it != cend(previousHashValues)) {
                        _defaultRenderStates[s][p][v] = it->second;
                    } else {
                        const size_t stateHash = RenderStateBlock::loadFromXML(Util::StringFormat("%s.RenderStates.%u", entryName.c_str(), stateIndex), pt);
                        _defaultRenderStates[s][p][v] = stateHash;
                        previousHashValues[stateIndex] = stateHash;
                    }
                }
            }
        }
    }
}

void Material::saveTextureDataToXML(const string& entryName, boost::property_tree::ptree& pt) const {
    hashMap<size_t, U32> previousHashValues;

    U32 samplerCount = 0u;
    for (const TextureUsage usage : g_materialTextures) {
        Texture_wptr tex = getTexture(usage);
        if (!tex.expired()) {
            const Texture_ptr texture = tex.lock();


            const string textureNode = entryName + ".texture." + TypeUtil::TextureUsageToString(usage);

            pt.put(textureNode + ".name", texture->assetName().str());
            pt.put(textureNode + ".path", texture->assetLocation().str());
            pt.put(textureNode + ".usage", TypeUtil::TextureOperationToString(_textureOperations[to_base(usage)]));

            const size_t samplerHash = _samplers[to_base(usage)];

            if (previousHashValues.find(samplerHash) == std::cend(previousHashValues)) {
                samplerCount++;
                XMLParser::saveToXML(SamplerDescriptor::get(samplerHash), Util::StringFormat("%s.SamplerDescriptors.%u", entryName.c_str(), samplerCount), pt);
                previousHashValues[samplerHash] = samplerCount;
            }
            pt.put(textureNode + ".Sampler.id", previousHashValues[samplerHash]);
            pt.put(textureNode + ".UseForPrePass", to_U32(_useForPrePass[to_base(usage)]));
        }
    }
}

void Material::loadTextureDataFromXML(const string& entryName, const boost::property_tree::ptree& pt) {
    hashMap<U32, size_t> previousHashValues;

    for (const TextureUsage usage : g_materialTextures) {
        if (pt.get_child_optional(entryName + ".texture." + TypeUtil::TextureUsageToString(usage) + ".name")) {
            const string textureNode = entryName + ".texture." + TypeUtil::TextureUsageToString(usage);

            const ResourcePath texName = ResourcePath(pt.get<string>(textureNode + ".name", ""));
            const ResourcePath texPath = ResourcePath(pt.get<string>(textureNode + ".path", ""));
            // May be a procedural texture
            if (texPath.empty()) {
                continue;
            }

            if (!texName.empty()) {
                _useForPrePass[to_base(usage)] = static_cast<TexturePrePassUsage>(pt.get<U32>(textureNode + ".UseForPrePass", to_U32(_useForPrePass[to_base(usage)])));
                const U32 index = pt.get<U32>(textureNode + ".Sampler.id", 0);
                const auto& it = previousHashValues.find(index);

                size_t hash = 0u;
                if (it != cend(previousHashValues)) {
                    hash = it->second;
                } else {
                     hash = XMLParser::loadFromXML(Util::StringFormat("%s.SamplerDescriptors.%u", entryName.c_str(), index), pt);
                     previousHashValues[index] = hash;
                }

                if (_samplers[to_base(usage)] != hash) {
                    setSampler(usage, hash);
                }

                TextureOperation& op = _textureOperations[to_base(usage)];
                op = TypeUtil::StringToTextureOperation(pt.get<string>(textureNode + ".usage", TypeUtil::TextureOperationToString(_textureOperations[to_base(usage)])));

                {
                    ScopedLock<SharedMutex> w_lock(_textureLock);
                    const Texture_ptr& crtTex = _textures[to_base(usage)];
                    if (crtTex == nullptr) {
                        op = TextureOperation::NONE;
                    } else if (crtTex->assetLocation() + crtTex->assetName() == texPath + texName) {
                        continue;
                    }
                }

                TextureDescriptor texDesc(TextureType::TEXTURE_2D_ARRAY);
                ResourceDescriptor texture(texName.str());
                texture.assetName(texName);
                texture.assetLocation(texPath);
                texture.propertyDescriptor(texDesc);
                texture.waitForReady(true);

                Texture_ptr tex = CreateResource<Texture>(_context.parent().resourceCache(), texture);
                setTexture(usage, tex, hash, op);
            }
        }
    }
}

};