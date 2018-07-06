#include "Headers/SSAOPreRenderOperator.h"
#include "Rendering/PostFX/Headers/PreRenderStageBuilder.h"
#include "Core/Resources/Headers/ResourceCache.h"
#include "Managers/Headers/SceneManager.h"
#include "Core/Headers/ParamHandler.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Geometry/Shapes/Headers/Predefined/Quad3D.h"

namespace Divide {

SSAOPreRenderOperator::SSAOPreRenderOperator(Framebuffer* result,
                                             const vec2<U16>& resolution,
                                             SamplerDescriptor* const sampler)
    : PreRenderOperator(PostFXRenderStage::SSAO, resolution, sampler), _outputFB(result)
{
    TextureDescriptor outputDescriptor(TextureType::TEXTURE_2D,
                                       GFXImageFormat::RGB8,
                                       GFXDataFormat::UNSIGNED_BYTE);
    outputDescriptor.setSampler(*_internalSampler);
    _outputFB->addAttachment(outputDescriptor, TextureDescriptor::AttachmentType::Color0);
    _outputFB->create(_resolution.width, _resolution.height);
    ResourceDescriptor ssao("SSAOPass");
    ssao.setThreadedLoading(false);
    _ssaoShader = CreateResource<ShaderProgram>(ssao);
}

SSAOPreRenderOperator::~SSAOPreRenderOperator() 
{
    RemoveResource(_ssaoShader);
}

void SSAOPreRenderOperator::reshape(U16 width, U16 height) {
    _outputFB->create(width, height);
}

void SSAOPreRenderOperator::operation() {
    if (!_enabled) return;

    _outputFB->begin(Framebuffer::defaultPolicy());
    _inputFB[0]->bind(0);                            // screen
    _inputFB[1]->bind(1, TextureDescriptor::AttachmentType::Depth);  // depth
    GFX_DEVICE.drawTriangle(GFX_DEVICE.getDefaultStateBlock(true),
                            _ssaoShader);
    _outputFB->end();
}
};