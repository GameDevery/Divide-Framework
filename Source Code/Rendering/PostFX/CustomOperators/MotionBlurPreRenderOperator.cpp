#include "stdafx.h"

#include "Headers/MotionBlurPreRenderOperator.h"

#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/Textures/Headers/Texture.h"
#include "Platform/Video/Headers/CommandBuffer.h"

#include "Core/Headers/Configuration.h"
#include "Core/Headers/Kernel.h"
#include "Core/Headers/PlatformContext.h"
#include "Core/Resources/Headers/ResourceCache.h"
#include "Geometry/Shapes/Predefined/Headers/Quad3D.h"

#include "Platform/Video/Buffers/RenderTarget/Headers/RenderTarget.h"
#include "Platform/Video/Shaders/Headers/ShaderProgram.h"
#include "Rendering/PostFX/Headers/PreRenderBatch.h"

namespace Divide {

MotionBlurPreRenderOperator::MotionBlurPreRenderOperator(GFXDevice& context, PreRenderBatch& parent, ResourceCache* cache)
    : PreRenderOperator(context, parent, FilterType::FILTER_MOTION_BLUR)
{
    ShaderModuleDescriptor vertModule = {};
    vertModule._moduleType = ShaderType::VERTEX;
    vertModule._sourceFile = "baseVertexShaders.glsl";
    vertModule._variant = "FullScreenQuad";

    ShaderModuleDescriptor fragModule = {};
    fragModule._moduleType = ShaderType::FRAGMENT;
    fragModule._sourceFile = "blur.glsl";
    fragModule._variant = "ObjectMotionBlur";

    ShaderProgramDescriptor shaderDescriptor = {};
    shaderDescriptor._modules.push_back(vertModule);
    shaderDescriptor._modules.push_back(fragModule);
    shaderDescriptor._globalDefines.emplace_back( "dvd_velocityScale PushData0[0].x" );
    shaderDescriptor._globalDefines.emplace_back( "dvd_maxSamples int(PushData0[0].y)" );

    ResourceDescriptor motionBlur("MotionBlur");
    motionBlur.propertyDescriptor(shaderDescriptor);
    motionBlur.waitForReady(false);

    _blurApply = CreateResource<ShaderProgram>(cache, motionBlur);
    _blurApply->addStateCallback(ResourceState::RES_LOADED, [this](CachedResource*)
    {
        PipelineDescriptor pipelineDescriptor = {};
        pipelineDescriptor._stateHash = _context.get2DStateBlock();
        pipelineDescriptor._shaderProgramHandle = _blurApply->handle();
        pipelineDescriptor._primitiveTopology = PrimitiveTopology::TRIANGLES;

        _blurApplyPipelineCmd._pipeline = _context.newPipeline(pipelineDescriptor);
    });

    parametersChanged();
}

bool MotionBlurPreRenderOperator::ready() const noexcept
{
    if (_blurApplyPipelineCmd._pipeline != nullptr)
    {
        return PreRenderOperator::ready();
    }

    return false;
}

void MotionBlurPreRenderOperator::parametersChanged() noexcept
{
    NOP();
}

bool MotionBlurPreRenderOperator::execute([[maybe_unused]] const PlayerIndex idx, const CameraSnapshot& cameraSnapshot, const RenderTargetHandle& input, const RenderTargetHandle& output, GFX::CommandBuffer& bufferInOut)
{
    const auto& screenAtt = input._rt->getAttachment(RTAttachmentType::COLOUR, GFXDevice::ScreenTargets::ALBEDO);
    const auto& velocityAtt = _parent.screenRT()._rt->getAttachment(RTAttachmentType::COLOUR, GFXDevice::ScreenTargets::VELOCITY);

    const F32 fps = _context.parent().platformContext().app().timer().getFps();
    const F32 velocityScale = _context.context().config().rendering.postFX.motionBlur.velocityScale;
    const F32 velocityFactor = fps / Config::TARGET_FRAME_RATE * velocityScale;

    auto cmd = GFX::EnqueueCommand<GFX::BindShaderResourcesCommand>(bufferInOut);
    cmd->_usage = DescriptorSetUsage::PER_DRAW;
    {
        DescriptorSetBinding& binding = AddBinding( cmd->_bindings, 0u, ShaderStageVisibility::FRAGMENT );
        Set( binding._data, screenAtt->texture()->sampledView(), screenAtt->descriptor()._samplerHash );
    }
    {
        DescriptorSetBinding& binding = AddBinding( cmd->_bindings, 1u, ShaderStageVisibility::FRAGMENT );
        Set( binding._data, velocityAtt->texture()->sampledView(), velocityAtt->descriptor()._samplerHash );
    }

    GFX::BeginRenderPassCommand* beginRenderPassCmd = GFX::EnqueueCommand<GFX::BeginRenderPassCommand>(bufferInOut);
    beginRenderPassCmd->_name = "DO_MOTION_BLUR_PASS";
    beginRenderPassCmd->_target = output._targetID;
    beginRenderPassCmd->_descriptor = _screenOnlyDraw;

    GFX::EnqueueCommand(bufferInOut, _blurApplyPipelineCmd);

    PushConstantsStruct params{};
    params.data[0]._vec[0].xy.set( velocityFactor, to_F32( maxSamples() ) );
    GFX::EnqueueCommand<GFX::SendPushConstantsCommand>(bufferInOut)->_constants.set(params);

    GFX::EnqueueCommand<GFX::DrawCommand>(bufferInOut);
    GFX::EnqueueCommand<GFX::EndRenderPassCommand>(bufferInOut);

    return true;
}
}
