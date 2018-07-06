#include "Headers/d3dTexture.h"
#include "Platform/Video/Direct3D/Headers/d3dEnumTable.h"

namespace Divide {

d3dTexture::d3dTexture(GFXDevice& context, TextureType type, bool asyncLoad)
    : Texture(context, type, asyncLoad)
{
    _type = d3dTextureTypeTable[to_uint(type)];
}

};