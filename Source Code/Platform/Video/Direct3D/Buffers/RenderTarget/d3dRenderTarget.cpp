#include "Headers/d3dRenderTarget.h"

namespace Divide {

IMPLEMENT_ALLOCATOR(d3dRenderTarget, 0, 0)
d3dRenderTarget::d3dRenderTarget(GFXDevice& context, bool multisampled)
    : RenderTarget(context, multisampled)
{
}

d3dRenderTarget::~d3dRenderTarget()
{
}

bool d3dRenderTarget::create(U16 width, U16 height) { return true; }

void d3dRenderTarget::destroy() {}

void d3dRenderTarget::begin(const RTDrawDescriptor& drawPolicy) {}

void d3dRenderTarget::end() {
}

void d3dRenderTarget::bind(U8 unit, RTAttachment::Type type, U8 index, bool flushStateOnRequest) {}

void d3dRenderTarget::drawToLayer(RTAttachment::Type type,
                                  U8 index,
                                  U32 layer,
                                  bool includeDepth) {}

void d3dRenderTarget::setMipLevel(U16 mipMinLevel, U16 mipMaxLevel, U16 writeLevel,
                                  RTAttachment::Type type, U8 index) {}

void d3dRenderTarget::resetMipLevel(RTAttachment::Type type, U8 index) {}

bool d3dRenderTarget::checkStatus() const { return true; }

void d3dRenderTarget::readData(const vec4<U16>& rect,
                               GFXImageFormat imageFormat,
                               GFXDataFormat dataType, bufferPtr outData) {}

void d3dRenderTarget::blitFrom(RenderTarget* inputFB,
                               bool blitColour,
                               bool blitDepth) {}

void d3dRenderTarget::blitFrom(RenderTarget* inputFB,
                               U8 index,
                               bool blitColour, bool blitDepth) {}

void d3dRenderTarget::clear(const RTDrawDescriptor& drawPolicy) const {}

};
