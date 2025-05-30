#pragma once
#ifndef DVD_PRE_RENDER_OPERATOR_H_
#define DVD_PRE_RENDER_OPERATOR_H_

#include "Platform/Video/Buffers/RenderTarget/Headers/RenderTarget.h"
#include "Platform/Video/Textures/Headers/Texture.h"


namespace Divide {
struct Configuration;
struct CameraSnapshot;

class Quad3D;
class Camera;
class Texture;
class Pipeline;
class ShaderProgram;

namespace GFX {
    class CommandBuffer;
};


enum class RenderStage : U8;

enum class FilterType : U16 {
    FILTER_SS_ANTIALIASING,
    FILTER_SS_AMBIENT_OCCLUSION,
    FILTER_SS_REFLECTIONS,
    FILTER_DEPTH_OF_FIELD,
    FILTER_MOTION_BLUR,
    FILTER_BLOOM,
    FILTER_LUT_CORECTION,
    FILTER_UNDERWATER,
    FILTER_NOISE,
    FILTER_VIGNETTE,
    FILTER_COUNT
};

enum class FilterSpace : U8 {
    // HDR Space: operators that work AND MODIFY the HDR screen target (e.g. SSAO, SSR)
    FILTER_SPACE_PRE_PASS = 0,
    // HDR Space: operators that work on the HDR target (e.g. Bloom, DoF)
    FILTER_SPACE_HDR,
    // LDR Space: operators that work on the post-tonemap target (e.g. Post-AA)
    FILTER_SPACE_LDR,
    // Post Effects: operators that just overlay the final image before presenting (vignette, udnerwarter effect, etc)
    FILTER_SPACE_POST_FX,
    COUNT
};

class PreRenderBatch;
/// It's called a prerender operator because it operates on the buffer before "rendering" to the screen
/// Technically, it's a post render operation
NOINITVTABLE_CLASS(PreRenderOperator)
{
   public:
    /// The RenderStage is used to inform the GFXDevice of what we are currently
    /// doing to set up appropriate states
    /// The target is the full screen quad to which we want to apply our
    /// operation to generate the result
    PreRenderOperator(GFXDevice& context, PreRenderBatch& parent, FilterType operatorType, std::atomic_uint& taskCounter);
    virtual ~PreRenderOperator() = default;

    /// Return true if we rendered into "output"
    virtual bool execute(PlayerIndex idx, const CameraSnapshot& cameraSnapshot, const RenderTargetHandle& input, const RenderTargetHandle& output, GFX::CommandBuffer& bufferInOut);

    virtual void reshape(U16 width, U16 height);

    virtual void prepare(PlayerIndex idx, GFX::CommandBuffer& bufferInOut);

    [[nodiscard]] FilterType operatorType() const noexcept { return _operatorType; }

    virtual void onToggle(bool state);

    [[nodiscard]] virtual bool ready() const noexcept { return true; }

   protected:
    GFXDevice& _context;

    PreRenderBatch& _parent;
    RTDrawDescriptor _screenOnlyDraw{};
    FilterType  _operatorType = FilterType::FILTER_COUNT;
    bool _enabled = true;
};

FWD_DECLARE_MANAGED_CLASS(PreRenderOperator);

};  // namespace Divide

#endif //DVD_PRE_RENDER_OPERATOR_H_
