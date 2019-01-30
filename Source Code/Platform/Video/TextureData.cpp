#include "stdafx.h"

#include "Headers/TextureData.h"

#include "Core/Headers/Console.h"
#include "Core/Math/Headers/MathHelper.h"
#include "Utility/Headers/Localization.h"

#include "Platform/Video/Shaders/Headers/ShaderProgram.h"

namespace Divide {

namespace {
    size_t GetHash(TextureData data) {
        size_t hash = 17;
        Util::Hash_combine(hash, to_U32(data._textureType));
        Util::Hash_combine(hash, data._samplerHandle);
        Util::Hash_combine(hash, data._textureHandle);
        return hash;
    }
};

TextureDataContainer::TextureDataContainer() noexcept
    : _textures{}
{
    _textures.reserve(to_base(ShaderProgram::TextureUsage::COUNT));
}

bool TextureDataContainer::set(const TextureDataContainer& other) {
    // EASTL should be fast enough to handle this
    const vectorEASTL<eastl::pair<TextureData, U8>>& otherTextures = other.textures();
    if (_textures != otherTextures) {
        _textures = otherTextures;
        return true;
    }

    return false;
}

bool TextureDataContainer::setTexture(const eastl::pair<TextureData, U8 /*binding*/>& textureEntry) {
    return setTexture(textureEntry.first, textureEntry.second);
}

bool TextureDataContainer::setTexture(const TextureData& data, U8 binding) {
    assert(data.type() != TextureType::COUNT);

    auto it = eastl::find_if(eastl::begin(_textures),
                             eastl::end(_textures),
                             [&binding](const eastl::pair<TextureData, U8>& textureData) {
                                return (textureData.second == binding);
                             });
    if (it == eastl::cend(_textures)) {
        _textures.emplace_back(data, binding);
        return false;
    }

    it->first = data;
    return true;
}

bool TextureDataContainer::setTextures(const TextureDataContainer& textureEntries) {
    return setTextures(textureEntries._textures);
}

bool TextureDataContainer::setTextures(const vectorEASTL<eastl::pair<TextureData, U8 /*binding*/>>& textureEntries) {
    bool ret = false;
    for (auto entry : textureEntries) {
        ret = setTexture(entry) || ret;
    }

    return ret;
}

bool TextureDataContainer::removeTexture(U8 binding) {
    vectorEASTL<eastl::pair<TextureData, U8>>::iterator it;
    it = eastl::find_if(eastl::begin(_textures), eastl::end(_textures),
                      [&binding](const eastl::pair<TextureData, U8>& entry)
    {
        return (entry.second == binding);
    });

    if (it != eastl::end(_textures)) {
        _textures.erase(it);
        return true;
    }

    return false;
}

bool TextureDataContainer::removeTexture(const TextureData& data) {
    size_t inputHash = GetHash(data);

    vectorEASTL<eastl::pair<TextureData, U8>>::iterator it;
    it = eastl::find_if(eastl::begin(_textures), eastl::end(_textures),
                      [&inputHash](const eastl::pair<TextureData, U8>& entry) {
                          return (GetHash(entry.first) == inputHash);
                      });

    if (it != eastl::end(_textures)) {
        _textures.erase(it);
        return true;
    }

    return false;
}

void TextureDataContainer::clear(bool clearMemory) {
    if (clearMemory) {
        _textures.clear();
    } else {
        _textures.resize(0);
    }
}

}; //namespace Divide