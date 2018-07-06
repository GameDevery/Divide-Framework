#include "stdafx.h"

#include "Headers/d3dEnumTable.h"

namespace Divide {

std::array<U32, to_base(TextureType::COUNT)> d3dTextureTypeTable;

void fillEnumTables() {
    d3dTextureTypeTable[to_U32(TextureType::TEXTURE_1D)] = 0;
    d3dTextureTypeTable[to_U32(TextureType::TEXTURE_2D)] = 1;
    d3dTextureTypeTable[to_U32(TextureType::TEXTURE_3D)] = 2;
    d3dTextureTypeTable[to_U32(TextureType::TEXTURE_CUBE_MAP)] = 3;
    d3dTextureTypeTable[to_U32(TextureType::TEXTURE_2D_ARRAY)] = 4;
    d3dTextureTypeTable[to_U32(TextureType::TEXTURE_CUBE_ARRAY)] = 5;
    d3dTextureTypeTable[to_U32(TextureType::TEXTURE_2D_MS)] = 6;
    d3dTextureTypeTable[to_U32(TextureType::TEXTURE_2D_ARRAY_MS)] = 7;
}
};