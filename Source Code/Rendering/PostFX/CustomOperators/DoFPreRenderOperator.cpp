#include "stdafx.h"

#include "Headers/DoFPreRenderOperator.h"

#include "Core/Headers/StringHelper.h"
#include "Core/Headers/Configuration.h"
#include "Core/Headers/PlatformContext.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Core/Resources/Headers/ResourceCache.h"
#include "Geometry/Shapes/Predefined/Headers/Quad3D.h"
#include "Platform/Video/Headers/CommandBuffer.h"
#include "Platform/Video/Shaders/Headers/ShaderProgram.h"

#include "Rendering/PostFX/Headers/PreRenderBatch.h"

namespace Divide {

namespace {
    constexpr U8 g_samplesOnFirstRing = 3;
    constexpr U8 g_ringCount = 4;
}

DoFPreRenderOperator::DoFPreRenderOperator(GFXDevice& context, PreRenderBatch& parent, ResourceCache* cache)
    : PreRenderOperator(context, parent, FilterType::FILTER_DEPTH_OF_FIELD)
{
    ShaderModuleDescriptor vertModule = {};
    vertModule._moduleType = ShaderType::VERTEX;
    vertModule._sourceFile = "baseVertexShaders.glsl";
    vertModule._variant = "FullScreenQuad";

    ShaderModuleDescriptor fragModule = {};
    fragModule._moduleType = ShaderType::FRAGMENT;
    fragModule._sourceFile = "DepthOfField.glsl";
    //blur the depth buffer?
    fragModule._defines.emplace_back("USE_DEPTH_BLUR");
    //use noise instead of pattern for sample dithering
    fragModule._defines.emplace_back("USER_NOISE");
    //use pentagon as bokeh shape?
    //fragModule._defines.emplace_back("USE_PENTAGON");
    fragModule._defines.emplace_back(Util::StringFormat("RING_COUNT %d", g_ringCount));
    fragModule._defines.emplace_back(Util::StringFormat("FIRST_RING_SAMPLES %d", g_samplesOnFirstRing));

    ShaderProgramDescriptor shaderDescriptor = {};
    shaderDescriptor._modules.push_back(vertModule);
    shaderDescriptor._modules.push_back(fragModule);

    ResourceDescriptor dof("DepthOfField");
    dof.waitForReady(false);
    dof.propertyDescriptor(shaderDescriptor);
    _dofShader = CreateResource<ShaderProgram>(cache, dof);
    _dofShader->addStateCallback(ResourceState::RES_LOADED, [this](CachedResource*)
    {
        PipelineDescriptor pipelineDescriptor = {};
        pipelineDescriptor._stateHash = _context.get2DStateBlock();
        pipelineDescriptor._shaderProgramHandle = _dofShader->handle();
        pipelineDescriptor._primitiveTopology = PrimitiveTopology::TRIANGLES;

        _pipeline = _context.newPipeline(pipelineDescriptor);
    });

    _constants.set(_ID("size"), GFX::PushConstantType::VEC2, _parent.screenRT()._rt->getResolution());

    parametersChanged();
}

bool DoFPreRenderOperator::ready() const noexcept
{
    if (_dofShader->getState() == ResourceState::RES_LOADED)
    {
        return PreRenderOperator::ready();
    }

    return false;
}

void DoFPreRenderOperator::parametersChanged() noexcept
{
    const auto& params = _context.context().config().rendering.postFX.dof;

    _constants.set(_ID("focus"), GFX::PushConstantType::VEC2, params.focalPoint);
    _constants.set(_ID("focalDepth"), GFX::PushConstantType::FLOAT, params.focalDepth);
    _constants.set(_ID("focalLength"), GFX::PushConstantType::FLOAT, params.focalLength);
    _constants.set(_ID("fstop"), GFX::PushConstantType::FLOAT, g_FStopValues[to_base(TypeUtil::StringToFStops(params.fStop))]);
    _constants.set(_ID("ndofstart "), GFX::PushConstantType::FLOAT, params.ndofstart);
    _constants.set(_ID("ndofdist"), GFX::PushConstantType::FLOAT, params.ndofdist);
    _constants.set(_ID("fdofstart"), GFX::PushConstantType::FLOAT, params.fdofstart);
    _constants.set(_ID("fdofdist"), GFX::PushConstantType::FLOAT, params.fdofdist);
    _constants.set(_ID("autofocus "), GFX::PushConstantType::BOOL, params.autoFocus);
    _constants.set(_ID("vignetting "), GFX::PushConstantType::BOOL, params.vignetting);
    _constants.set(_ID("showFocus"), GFX::PushConstantType::BOOL, params.debugFocus);
    _constants.set(_ID("manualdof "), GFX::PushConstantType::BOOL, params.manualdof);

    _constantsDirty = true;
}

void DoFPreRenderOperator::reshape(const U16 width, const U16 height)
{
    PreRenderOperator::reshape(width, height);
    _constants.set(_ID("size"), GFX::PushConstantType::VEC2, vec2<F32>(width, height));
    _constantsDirty = true;
}

bool DoFPreRenderOperator::execute([[maybe_unused]] const PlayerIndex idx, const CameraSnapshot& cameraSnapshot, const RenderTargetHandle& input, const RenderTargetHandle& output, GFX::CommandBuffer& bufferInOut)
{
    const vec2<F32>& zPlanes = cameraSnapshot._zPlanes;
    if (_cachedZPlanes != zPlanes)
    {
        _constants.set(_ID("_zPlanes"), GFX::PushConstantType::VEC2, zPlanes);
        _cachedZPlanes = zPlanes;
        _constantsDirty = true;
    }
    
    const auto& screenAtt = input._rt->getAttachment(RTAttachmentType::COLOUR, GFXDevice::ScreenTargets::ALBEDO);
    const auto& extraAtt = _parent.getLinearDepthRT()._rt->getAttachment(RTAttachmentType::COLOUR);
    const auto& screenTex = screenAtt->texture()->sampledView();
    const auto& extraTex = extraAtt->texture()->sampledView();

    auto cmd = GFX::EnqueueCommand<GFX::BindShaderResourcesCommand>(bufferInOut);
    cmd->_usage = DescriptorSetUsage::PER_DRAW;
    {
        auto& binding = cmd->_bindings.emplace_back(ShaderStageVisibility::FRAGMENT);
        binding._slot = 0;
        binding._data.As<DescriptorCombinedImageSampler>() = { screenTex, screenAtt->descriptor()._samplerHash };
    }
    {
        auto& binding = cmd->_bindings.emplace_back(ShaderStageVisibility::FRAGMENT);
        binding._slot = 1;
        binding._data.As<DescriptorCombinedImageSampler>() = { extraTex, extraAtt->descriptor()._samplerHash };
    }

    GFX::BeginRenderPassCommand beginRenderPassCmd{};
    beginRenderPassCmd._target = output._targetID;
    beginRenderPassCmd._descriptor = _screenOnlyDraw;
    beginRenderPassCmd._name = "DO_DOF_PASS";
    GFX::EnqueueCommand(bufferInOut, beginRenderPassCmd);

    GFX::EnqueueCommand(bufferInOut, GFX::BindPipelineCommand{ _pipeline });

    if (_constantsDirty) {
        GFX::EnqueueCommand(bufferInOut, GFX::SendPushConstantsCommand{ _constants });
        _constantsDirty = false;
    }

    GFX::EnqueueCommand<GFX::DrawCommand>(bufferInOut);
    GFX::EnqueueCommand<GFX::EndRenderPassCommand>(bufferInOut);

    return true;
}
}
