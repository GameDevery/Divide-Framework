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
#ifndef DVD_MATERIAL_H_
#define DVD_MATERIAL_H_

#include "MaterialEnums.h"

#include "Utility/Headers/XMLParser.h"
#include "Utility/Headers/Colours.h"

#include "Core/Resources/Headers/Resource.h"

#include "Platform/Video/Headers/RenderStagePass.h"
#include "Platform/Video/Headers/RenderStateBlock.h"
#include "Platform/Video/Headers/AttributeDescriptor.h"
#include "Platform/Video/Textures/Headers/Texture.h"
#include "Platform/Video/Shaders/Headers/ShaderProgramFwd.h"
#include "Platform/Video/Textures/Headers/SamplerDescriptor.h"

#include "Geometry/Material/Headers/ShaderProgramInfo.h"
#include "Rendering/RenderPass/Headers/NodeBufferedData.h"

namespace Divide {

namespace Attorney {
    class MaterialRenderBin;
}

class RenderBin;
class RenderingComponent;

struct NodeMaterialData;
struct RenderStateBlock;

enum class BlendProperty : U8;
enum class ReflectorType : U8;
enum class RefractorType : U8;

constexpr F32 Specular_Glass = 0.5f;
constexpr F32 Specular_Plastic = 0.5f;
constexpr F32 Specular_Quarts = 0.57f;
constexpr F32 Specular_Ice = 0.224f;
constexpr F32 Specular_Water = 0.255f;
constexpr F32 Specular_Milk = 0.277f;
constexpr F32 Specular_Skin = 0.35f;

enum class TextureSlot : U8 {
    UNIT0 = 0,
    OPACITY,
    NORMALMAP,
    HEIGHTMAP,
    SPECULAR,
    METALNESS,
    ROUGHNESS,
    OCCLUSION,
    EMISSIVE,
    UNIT1,
    COUNT
};
namespace Names {
    static constexpr const char* textureSlot[] = {
        "UNIT0",
        "OPACITY",
        "NORMALMAP",
        "HEIGHTMAP",
        "SPECULAR",
        "METALNESS",
        "ROUGHNESS",
        "OCCLUSION",
        "EMISSIVE",
        "UNIT1",
        "NONE"
    };
};

static_assert(std::size(Names::textureSlot) == to_base(TextureSlot::COUNT) + 1);


namespace TypeUtil {
    [[nodiscard]] const char* MaterialDebugFlagToString(const MaterialDebugFlag unitType) noexcept;
    [[nodiscard]] MaterialDebugFlag StringToMaterialDebugFlag(std::string_view name);

    [[nodiscard]] const char* ShadingModeToString(ShadingMode shadingMode) noexcept;
    [[nodiscard]] ShadingMode StringToShadingMode(std::string_view name);

    [[nodiscard]] const char* SkinningModeToString(SkinningMode skinningMode) noexcept;
    [[nodiscard]] SkinningMode StringToSkinningMode(std::string_view name);

    [[nodiscard]] const char* TextureSlotToString(TextureSlot texUsage) noexcept;
    [[nodiscard]] TextureSlot StringToTextureSlot(std::string_view name);

    [[nodiscard]] const char* TextureOperationToString(TextureOperation textureOp) noexcept;
    [[nodiscard]] TextureOperation StringToTextureOperation(std::string_view operation);

    [[nodiscard]] const char* BumpMethodToString(BumpMethod bumpMethod) noexcept;
    [[nodiscard]] BumpMethod StringToBumpMethod(std::string_view name);

    [[nodiscard]] const char* ReflectorTypeToString(ReflectorType reflectorType) noexcept;
    [[nodiscard]] ReflectorType StringToReflectorType(std::string_view name);

    [[nodiscard]] const char* RefractorTypeToString(RefractorType refractorType) noexcept;
    [[nodiscard]] RefractorType StringToRefractorType(std::string_view name);
};

class Material final : public CachedResource {
    friend class Attorney::MaterialRenderBin;

  public:
    static constexpr F32 MAX_SHININESS = 255.f;

    using SpecularGlossiness = float2;
    using ComputeShaderCBK = DELEGATE_STD<ShaderProgramDescriptor, Material*, RenderStagePass>;
    using ComputeRenderStateCBK = DELEGATE_STD<void, Material*, RenderStagePass, RenderStateBlock&>;
    using RecomputeShadersCBK = DELEGATE_STD<void>;

    template<typename T> using StatesPerVariant = eastl::array<T, to_base(RenderStagePass::VariantType::COUNT)>;
    template<typename T> using StateVariantsPerPass = eastl::array<StatesPerVariant<T>, to_base(RenderPassType::COUNT)>;
    template<typename T> using StatePassesPerStage = eastl::array<StateVariantsPerPass<T>, to_base(RenderStage::COUNT)>;

    enum class UpdatePriority : U8 {
        Default,
        Medium,
        High,
        COUNT
    };

    struct ShaderData {
        Str<64> _depthShaderVertSource = "baseVertexShaders";
        Str<32> _depthShaderVertVariant = "BasicLightData";
        Str<32> _shadowShaderVertVariant = "BasicData";

        Str<64> _colourShaderVertSource = "baseVertexShaders";
        Str<32> _colourShaderVertVariant = "BasicLightData";

        Str<64> _depthShaderFragSource = "depthPass";
        Str<32> _depthShaderFragVariant = "";

        Str<64> _colourShaderFragSource = "material";
        Str<32> _colourShaderFragVariant = "";
    };

    struct Properties
    {
        friend class Material;
        PROPERTY_R(FColour4, baseColour, DefaultColours::WHITE);

        PROPERTY_RW(FColour3, specular, DefaultColours::BLACK); 
        PROPERTY_RW(FColour3, emissive, DefaultColours::BLACK);
        PROPERTY_RW(FColour3, ambient, DefaultColours::BLACK);
        PROPERTY_RW(SpecularGlossiness, specGloss);
        PROPERTY_RW(F32, shininess, 0.f);
        PROPERTY_RW(F32, metallic, 0.f);
        PROPERTY_RW(F32, roughness, 0.5f);
        PROPERTY_RW(F32, occlusion, 1.0f);
        PROPERTY_RW(F32, parallaxFactor, 1.0f);

        PROPERTY_R(BumpMethod, bumpMethod, BumpMethod::NONE);

        PROPERTY_R(bool, receivesShadows, true);
        PROPERTY_R(bool, isStatic, false);
        PROPERTY_R(bool, isInstanced, false);
        PROPERTY_R(bool, texturesInFragmentStageOnly, true);
        PROPERTY_R(ReflectorType, reflectorType, ReflectorType::COUNT);
        PROPERTY_R(RefractorType, refractorType, RefractorType::COUNT);
        PROPERTY_R(bool, doubleSided, false);
        PROPERTY_R(SkinningMode, skinningMode, SkinningMode::COUNT);
        PROPERTY_R(ShadingMode, shadingMode, ShadingMode::COUNT);
        PROPERTY_R(TranslucencySource, translucencySource, TranslucencySource::COUNT);
        /// If the metalness textures has 3 (or 4) channels, those channels are interpreted automatically as R: Occlusion, G: Metalness, B: Roughness
        PROPERTY_R(bool, usePackedOMR, false);

        struct Overrides {
            friend class Divide::Material;

            PROPERTY_R_IW(bool, ignoreTexDiffuseAlpha, false);
            PROPERTY_R_IW(bool, transparencyEnabled, true);
            PROPERTY_R_IW(bool, useAlphaDiscard, true);
        };

        PROPERTY_RW(Overrides, overrides);

        PROPERTY_R_IW(bool, cullUpdated, false);
        PROPERTY_R_IW(bool, transparencyUpdated, false);
        PROPERTY_R_IW(bool, needsNewShader, true);

    public:
        void texturesInFragmentStageOnly(bool state) noexcept;
        void shadingMode(ShadingMode mode) noexcept;
        void skinningMode(SkinningMode mode) noexcept;
        void doubleSided(bool state) noexcept;
        void receivesShadows(bool state) noexcept;
        void reflectorType(ReflectorType state) noexcept;
        void refractorType(RefractorType state) noexcept;
        void isStatic(bool state) noexcept;
        void isInstanced(bool state) noexcept;
        void ignoreTexDiffuseAlpha(bool state) noexcept;
        void bumpMethod(BumpMethod newBumpMethod) noexcept;
        void toggleTransparency(bool state) noexcept;
        void useAlphaDiscard(bool state) noexcept;
        void baseColour(const FColour4& colour) noexcept;

    protected:
        void saveToXML(const std::string& entryName, boost::property_tree::ptree& pt) const;
        void loadFromXML(const std::string& entryName, const boost::property_tree::ptree& pt);
    };

    struct TextureInfo 
    {
        Handle<Texture> _ptr{ INVALID_HANDLE<Texture> };
        SamplerDescriptor _sampler{};
        TextureOperation _operation{ TextureOperation::NONE };
        bool _srgb{false};
        bool _useInGeometryPasses{false}; ///< Setting this to false will fallback to auto-usage selection (e.g. opacity tex will be used for alpha testing in shadow passes)
    };

   public:
    explicit Material( const ResourceDescriptor<Material>& descriptor );

    static void OnStartup();
    static void OnShutdown();
    static void RecomputeShaders();
    static void Update(U64 deltaTimeUS);
    static bool Clone(Handle<Material> src, Handle<Material>& dst, const std::string_view nameSuffix);

    /// Return a new instance of this material with the name composed of the base material's name and the give name suffix (clone calls CreateResource internally!).
    [[nodiscard]] bool load(PlatformContext& context) override;
    [[nodiscard]] bool unload() override;
    /// Returns a bit mask composed of UpdateResult flags
    [[nodiscard]] U32 update(U64 deltaTimeUS);

    void rebuild();
    void clearRenderStates();
    void updateCullState();

    void setPipelineLayout(PrimitiveTopology topology, const AttributeMap& shaderAttributes);

    bool setSampler(TextureSlot textureUsageSlot,
                    SamplerDescriptor sampler);

    bool setTexture(TextureSlot textureUsageSlot,
                    Handle<Texture> texture,
                    SamplerDescriptor sampler,
                    TextureOperation op,
                    bool useInGeometryPasses = false);

    bool setTexture( TextureSlot textureUsageSlot,
                     const ResourceDescriptor<Texture>& texture,
                     SamplerDescriptor sampler,
                     TextureOperation op,
                     bool useInGeometryPasses = false );
    void setTextureOperation(TextureSlot textureUsageSlot, TextureOperation op);

    void lockInstancesForRead() const noexcept;
    void unlockInstancesForRead() const noexcept;
    void lockInstancesForWrite() const noexcept;
    void unlockInstancesForWrite() const noexcept;

    [[nodiscard]] const vector<Handle<Material>>& getInstancesLocked() const noexcept;
    [[nodiscard]] const vector<Handle<Material>>& getInstances() const;

    /// Add the specified renderStateBlock to specific RenderStagePass parameters. Use "COUNT" and/or "g_AllVariantsID" for global options
    /// e.g. a RenderPassType::COUNT will use the block in the specified stage+variant combo but for all of the passes
    void setRenderStateBlock(const RenderStateBlock& renderStateBlock, RenderStage stage, RenderPassType pass, RenderStagePass::VariantType variant = RenderStagePass::VariantType::COUNT);

    // Returns the material's hash value (just for the uploadable data)
    void getData(U32 bestProbeID, NodeMaterialData& dataOut);

    [[nodiscard]] FColour4 getBaseColour(bool& hasTextureOverride, Handle<Texture>& textureOut) const noexcept;
    [[nodiscard]] FColour3 getEmissive(bool& hasTextureOverride, Handle<Texture>& textureOut) const noexcept;
    [[nodiscard]] FColour3 getAmbient(bool& hasTextureOverride, Handle<Texture>& textureOut) const noexcept;
    [[nodiscard]] FColour3 getSpecular(bool& hasTextureOverride, Handle<Texture>& textureOut) const noexcept;
    [[nodiscard]] F32 getMetallic(bool& hasTextureOverride, Handle<Texture>& textureOut) const noexcept;
    [[nodiscard]] F32 getRoughness(bool& hasTextureOverride, Handle<Texture>& textureOut) const noexcept;
    [[nodiscard]] F32 getOcclusion(bool& hasTextureOverride, Handle<Texture>& textureOut) const noexcept;
    [[nodiscard]] const TextureInfo& getTextureInfo(TextureSlot usage) const;
    [[nodiscard]] const RenderStateBlock& getOrCreateRenderStateBlock(RenderStagePass renderStagePass);
    [[nodiscard]] Handle<Texture> getTexture(TextureSlot textureUsage) const;
    [[nodiscard]] DescriptorSet& getDescriptorSet(const RenderStagePass& renderStagePass);
    [[nodiscard]] Handle<ShaderProgram> getProgramHandle(RenderStagePass renderStagePass) const;
    [[nodiscard]] Handle<ShaderProgram> computeAndGetProgramHandle(RenderStagePass renderStagePass);
    [[nodiscard]] bool hasTransparency() const noexcept;
    [[nodiscard]] bool isReflective() const noexcept;
    [[nodiscard]] bool isRefractive() const noexcept;
    [[nodiscard]] bool canDraw(RenderStagePass renderStagePass, bool& shaderJustFinishedLoading);
    [[nodiscard]] const ModuleDefines& shaderDefines(ShaderType type) const;

    // type == ShaderType::Count = add to all stages
    void addShaderDefine(ShaderType type, const string& define, bool addPrefix = true);
    void addShaderDefine(const string& define, bool addPrefix = true);
    void saveToXML( const std::string& entryName, boost::property_tree::ptree& pt) const;
    void loadFromXML( const std::string& entryName, const boost::property_tree::ptree& pt);

    PROPERTY_RW(Properties, properties);
    PROPERTY_RW(ShaderData, baseShaderData);
    PROPERTY_RW(ComputeShaderCBK, computeShaderCBK);
    PROPERTY_RW(ComputeRenderStateCBK, computeRenderStateCBK);
    PROPERTY_RW(RecomputeShadersCBK, recomputeShadersCBK);
    PROPERTY_R_IW(Handle<Material>, baseMaterial, INVALID_HANDLE<Material>);
    PROPERTY_RW(UpdatePriority, updatePriorirty, UpdatePriority::Default);
    PROPERTY_R_IW(DescriptorSet, descriptorSetSecondaryPass);
    PROPERTY_R_IW(DescriptorSet, descriptorSetMainPass);
    PROPERTY_R_IW(DescriptorSet, descriptorSetPrePass);
    PROPERTY_R_IW(DescriptorSet, descriptorSetShadow);
    PROPERTY_RW(bool, ignoreXMLData, false);
    PROPERTY_RW(bool, ignoreShaderCache, false); ///< Useful for debugging
    PROPERTY_R_IW(AttributeMap, shaderAttributes);
    PROPERTY_R_IW(PrimitiveTopology, topology, PrimitiveTopology::COUNT);
   private:
    void getSortKeys(RenderStagePass renderStagePass, I64& shaderKey, I64& textureKey, bool& transparencyFlag) const;
    void addShaderDefineInternal(ShaderType type, const string& define, bool addPrefix);

    void updateTransparency();

    void recomputeShaders();

    void setShaderProgramInternal(ShaderProgramDescriptor shaderDescriptor, RenderStagePass stagePass);

    void computeAndAppendShaderDefines( ShaderProgramDescriptor& shaderDescriptor, RenderStagePass renderStagePass) const;

    [[nodiscard]] ShaderProgramInfo& shaderInfo(RenderStagePass renderStagePass);

    [[nodiscard]] const ShaderProgramInfo& shaderInfo(RenderStagePass renderStagePass) const;

    void saveRenderStatesToXML(const std::string& entryName, boost::property_tree::ptree& pt) const;
    void loadRenderStatesFromXML(const std::string& entryName, const boost::property_tree::ptree& pt);

    void saveTextureDataToXML(const std::string& entryName, boost::property_tree::ptree& pt) const;
    void loadTextureDataFromXML(const std::string& entryName, const boost::property_tree::ptree& pt);

    bool setTextureLocked(TextureSlot textureUsageSlot,
                          Handle<Texture> texture,
                          SamplerDescriptor sampler,
                          TextureOperation op,
                          bool useInGeometryPasses);

    bool setTextureLocked(TextureSlot textureUsageSlot,
                          const ResourceDescriptor<Texture>& texture,
                          SamplerDescriptor sampler,
                          TextureOperation op,
                          bool useInGeometryPasses);

    [[nodiscard]] bool usesTextureInShader(TextureSlot slot, bool isPrePass, bool isShadowPass) const noexcept;

   private:
     GFXDevice* _context = nullptr;

    struct RenderStateBlockEntry
    {
        RenderStateBlock _block{};
        bool _isSet{false};
    };

    StatePassesPerStage<ShaderProgramInfo> _shaderInfo{};
    StatePassesPerStage<RenderStateBlockEntry> _defaultRenderStates{};

    std::array<ModuleDefines, to_base(ShaderType::COUNT)> _extraShaderDefines{};
    mutable SharedMutex _textureLock{};

    mutable SharedMutex _instanceLock;
    vector<Handle<Material>> _instances{};

    std::array<TextureInfo, to_base(TextureSlot::COUNT)> _textures;

    size_t _shaderAttributesHash{ 0u };

    static bool s_shadersDirty;
};

TYPEDEF_SMART_POINTERS_FOR_TYPE(Material);

namespace Attorney {
class MaterialRenderBin {
    static void getSortKeys(Material* material, const RenderStagePass renderStagePass, I64& shaderKey, I64& textureKey, bool& hasTransparency) {
        material->getSortKeys(renderStagePass, shaderKey, textureKey, hasTransparency);
    }

    friend class Divide::RenderBin;
};
} //namespace Attorney

};  // namespace Divide

#endif //DVD_MATERIAL_H_

#include "Material.inl"
