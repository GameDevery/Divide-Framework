#include "stdafx.h"

#include "Headers/RTAttachment.h"

#include "Headers/RenderTarget.h"

#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/Textures/Headers/Texture.h"

namespace Divide {

RTAttachment::RTAttachment(RenderTarget& parent, const RTAttachmentDescriptor& descriptor) noexcept
    : _parent(parent),
      _descriptor(descriptor)
{
}

const Texture_ptr& RTAttachment::texture() const {
    return _texture;
}

void RTAttachment::setTexture(const Texture_ptr& tex, const bool isExternal) noexcept {
    _texture = tex;
    if (tex != nullptr) {
        assert(IsValid(tex->data()));
    }
    changed(true);
}

bool RTAttachment::used() const noexcept {
    return _texture != nullptr;
}

bool RTAttachment::mipWriteLevel(const U16 level) noexcept {
    if (_texture != nullptr && _texture->mipCount() > level && _mipWriteLevel != level) {
        _mipWriteLevel = level;
        return true;
    }

    return false;
}

U16 RTAttachment::mipWriteLevel() const noexcept {
    return _mipWriteLevel;
}

bool RTAttachment::writeLayer(const U16 layer) {
    const U16 layerCount = IsCubeTexture(texture()->descriptor().texType()) ? numLayers() * 6 : numLayers();
    if (layerCount > layer && _writeLayer != layer) {
        _writeLayer = layer;
        return true;
    }

    return false;
}

U16 RTAttachment::writeLayer() const noexcept {
    return _writeLayer;
}

U16 RTAttachment::numLayers() const {
    return used() ? _texture->descriptor().layerCount() : 0u;
}

void RTAttachment::clearColour(const FColour4& clearColour) noexcept {
    _descriptor._clearColour.set(clearColour);
}

const FColour4& RTAttachment::clearColour() const noexcept {
    return _descriptor._clearColour;
}

const RTAttachmentDescriptor& RTAttachment::descriptor() const noexcept {
    return _descriptor;
}

RenderTarget& RTAttachment::parent() noexcept {
    return _parent;
}

const RenderTarget& RTAttachment::parent() const  noexcept {
    return _parent;
}


}; //namespace Divide