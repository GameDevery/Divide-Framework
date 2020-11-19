#pragma once
#ifndef _PRE_RenderStage_H_
#define _PRE_RenderStage_H_

#include "PreRenderOperator.h"

namespace Divide {

class PostFX;

struct ToneMapParams
{
    enum class MapFunctions : U8
    {
        REINHARD,
        REINHARD_MODIFIED,
        GT,
        ACES,
        UNREAL_ACES,
        AMD_ACES,
        GT_DIFF_PARAMETERS,
        UNCHARTED_2,
        COUNT
    };

    U16 _width = 1u;
    U16 _height = 1u;
    F32 _minLogLuminance = -4.0f;
    F32 _maxLogLuminance = 3.0f;
    F32 _tau = 1.1f;

    F32 _manualExposure = 11.2f;

    MapFunctions _function = MapFunctions::UNCHARTED_2;
};

namespace Names {
    static const char* toneMapFunctions[] = {
        "REINHARD", "REINHARD_MODIFIED", "GT", "ACES", "UNREAL_ACES", "AMD_ACES", "GT_DIFF_PARAMETERS", "UNCHARTED_2", "NONE"
    };
}

namespace TypeUtil {
    const char* ToneMapFunctionsToString(ToneMapParams::MapFunctions stop) noexcept;
    ToneMapParams::MapFunctions StringToToneMapFunctions(const stringImpl& name);
};

class ResourceCache;
class PreRenderBatch {
   public:
       // Ordered by cost
       enum class EdgeDetectionMethod : U8 {
           Depth = 0,
           Luma,
           Colour,
           COUNT
       };
   public:
    PreRenderBatch(GFXDevice& context, PostFX& parent, ResourceCache* cache);
    ~PreRenderBatch();

    [[nodiscard]] PostFX& parent() const noexcept { return _parent; }

    void idle(const Configuration& config);
    void update(U64 deltaTimeUS) noexcept;

    void prepare(const Camera* camera, U32 filterStack, GFX::CommandBuffer& bufferInOut);
    void execute(const Camera* camera, U32 filterStack, GFX::CommandBuffer& bufferInOut);
    void reshape(U16 width, U16 height);

    void onFilterEnabled(FilterType filter);
    void onFilterDisabled(FilterType filter);

    [[nodiscard]] RenderTargetHandle getInput(bool hdr) const;
    [[nodiscard]] RenderTargetHandle getOutput(bool hdr) const;

    [[nodiscard]] RenderTargetHandle screenRT() const noexcept;
    [[nodiscard]] RenderTargetHandle edgesRT() const noexcept;
    [[nodiscard]] Texture_ptr luminanceTex() const noexcept;

    [[nodiscard]] PreRenderOperator* getOperator(FilterType type) {
        const FilterSpace fSpace = GetOperatorSpace(type);
        if (fSpace == FilterSpace::COUNT) {
            return nullptr;
        }

        const OperatorBatch& batch = _operators[to_U32(fSpace)];
        const auto* const it = std::find_if(std::cbegin(batch), std::cend(batch), [type](const eastl::unique_ptr<PreRenderOperator>& op) noexcept {
                                    return op->operatorType() == type;
                              });

        assert(it != std::cend(batch));
        return (*it).get();
    }

    [[nodiscard]] const PreRenderOperator* getOperator(FilterType type) const {
        const FilterSpace fSpace = GetOperatorSpace(type);
        if (fSpace == FilterSpace::COUNT) {
            return nullptr;
        }

        const OperatorBatch& batch = _operators[to_U32(GetOperatorSpace(type))];
        const auto* const it = std::find_if(std::cbegin(batch), std::cend(batch), [type](const eastl::unique_ptr<PreRenderOperator>& op) noexcept {
                                    return op->operatorType() == type;
                              });
        assert(it != std::cend(batch));
        return (*it).get();
    }

    PROPERTY_R(bool, adaptiveExposureControl, true);
    PROPERTY_R(ToneMapParams, toneMapParams, {});
    PROPERTY_RW(F32, edgeDetectionThreshold, 0.1f);
    PROPERTY_RW(EdgeDetectionMethod, edgeDetectionMethod, EdgeDetectionMethod::Luma);

    void adaptiveExposureControl(bool state) noexcept;
    [[nodiscard]] F32  adaptiveExposureValue() const;

    void toneMapParams(ToneMapParams params) noexcept;

   private:

    [[nodiscard]] static FilterSpace GetOperatorSpace(const FilterType type) noexcept {
        // ToDo: Always keep this up-to-date with every filter we add
        switch(type) {
            case FilterType::FILTER_SS_ANTIALIASING :
                return FilterSpace::FILTER_SPACE_LDR;

            case FilterType::FILTER_SS_AMBIENT_OCCLUSION:
            case FilterType::FILTER_DEPTH_OF_FIELD:
            case FilterType::FILTER_BLOOM:
            case FilterType::FILTER_MOTION_BLUR:
                return FilterSpace::FILTER_SPACE_HDR;

            case FilterType::FILTER_SS_REFLECTIONS:
            case FilterType::FILTER_LUT_CORECTION:
            case FilterType::FILTER_COUNT:
            case FilterType::FILTER_UNDERWATER: 
            case FilterType::FILTER_NOISE: 
            case FilterType::FILTER_VIGNETTE: break;
        }

        return FilterSpace::COUNT;
    }

    void onFilterToggle(FilterType filter, bool state);

    [[nodiscard]] bool operatorsReady() const;

    [[nodiscard]] RenderTargetHandle getTarget(bool hdr, bool swapped) const;
  private:
    using OperatorBatch = vectorEASTL<eastl::unique_ptr<PreRenderOperator>>;
    std::array<OperatorBatch, to_base(FilterSpace::COUNT)> _operators;

    GFXDevice& _context;
    PostFX&    _parent;
    ResourceCache* _resCache = nullptr;

    ShaderBuffer*      _histogramBuffer = nullptr;

    struct HDRTargets {
        RenderTargetHandle _screenRef;
        RenderTargetHandle _screenCopy;
    };
    struct LDRTargets {
        RenderTargetHandle _temp[2];
    };

    struct ScreenTargets {
        HDRTargets _hdr;
        LDRTargets _ldr;
        bool _swappedHDR = false;
        bool _swappedLDR = false;
    };

    ScreenTargets _screenRTs;

    RenderTargetHandle _sceneEdges;
    Texture_ptr _currentLuminance;
    ShaderProgram_ptr _toneMap = nullptr;
    ShaderProgram_ptr _toneMapAdaptive = nullptr;
    ShaderProgram_ptr _createHistogram = nullptr;
    ShaderProgram_ptr _averageHistogram = nullptr;
    std::array<ShaderProgram_ptr, to_base(EdgeDetectionMethod::COUNT)> _edgeDetection = {};
    std::array<Pipeline*, to_base(EdgeDetectionMethod::COUNT)> _edgeDetectionPipelines = {};

    PushConstants     _toneMapConstants;

    U64 _lastDeltaTimeUS = 0u;
    bool _toneMapParamsDirty = true;
};

}  // namespace Divide
#endif
