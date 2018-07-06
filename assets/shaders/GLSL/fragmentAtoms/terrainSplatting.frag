#ifndef _TERRAIN_SPLATTING_FRAG_
#define _TERRAIN_SPLATTING_FRAG_

#include "bumpMapping.frag"

uniform vec4 diffuseScale[MAX_TEXTURE_LAYERS];
uniform vec4 detailScale[MAX_TEXTURE_LAYERS];

layout(binding = TEXTURE_UNIT0)     uniform sampler2D texWaterCaustics;
layout(binding = TEXTURE_UNIT1)     uniform sampler2D texUnderwaterAlbedo;
layout(binding = TEXTURE_NORMALMAP) uniform sampler2D texUnderwaterDetail;
layout(binding = TEXTURE_OPACITY)   uniform sampler2D texHeightMap;

uniform sampler2D      texBlend[MAX_TEXTURE_LAYERS];
uniform sampler2DArray texTileMaps[MAX_TEXTURE_LAYERS];
uniform sampler2DArray texNormalMaps[MAX_TEXTURE_LAYERS];

vec4 getFinalColour1(const in vec4 blendMap, const in uint index, const in vec4 diffSize) {
    return texture(texTileMaps[index], vec3(scaledTextureCoords(VAR._texCoord, diffSize.r), 0));
}

vec3 getFinalTBN1(const in vec4 blendMap, const in uint index, const in vec4 normSize){
    return texture(texNormalMaps[index], vec3(scaledTextureCoords(VAR._texCoord, normSize.r), 0)).rgb;
}

vec4 getFinalColour2(const in vec4 blendMap, const in uint index, const in vec4 diffSize){
    return mix(getFinalColour1(blendMap, index, diffSize), 
               texture(texTileMaps[index], vec3(scaledTextureCoords(VAR._texCoord, diffSize.g), 1)),
               blendMap.g);
}

vec3 getFinalTBN2(const in vec4 blendMap, const in uint index, const in vec4 normSize){
    return mix(getFinalTBN1(blendMap, index, normSize), 
               texture(texNormalMaps[index], vec3(scaledTextureCoords(VAR._texCoord, normSize.g), 1)).rgb,
               blendMap.g);
}

vec4 getFinalColour3(const in vec4 blendMap, const in uint index, const in vec4 diffSize){
    return mix(getFinalColour2(blendMap, index, diffSize), 
               texture(texTileMaps[index], vec3(scaledTextureCoords(VAR._texCoord, diffSize.b), 2)),
               blendMap.b);
}

vec3 getFinalTBN3(const in vec4 blendMap, const in uint index, const in vec4 normSize){
    return mix(getFinalTBN2(blendMap, index, normSize), 
               texture(texNormalMaps[index], vec3(scaledTextureCoords(VAR._texCoord, normSize.b), 2)).rgb,
               blendMap.b);
}

vec4 getFinalColour4(const in vec4 blendMap, const in uint index, const in vec4 diffSize){
    return mix(getFinalColour3(blendMap, index, diffSize), 
               texture(texTileMaps[index], vec3(scaledTextureCoords(VAR._texCoord, diffSize.a), 3)),
               blendMap.a);
}

vec3 getFinalTBN4(const in vec4 blendMap, const in uint index, const in vec4 normSize){
    return mix(getFinalTBN3(blendMap, index, normSize), 
               texture(texNormalMaps[index], vec3(scaledTextureCoords(VAR._texCoord, normSize.a), 3)).rgb,
               blendMap.a);
}

vec4 getTerrainAlbedo(){
    vec4 blendMap;
    vec4 colour = vec4(0.0);
    
    for (uint i = 0; i < MAX_TEXTURE_LAYERS; i++) {
        blendMap = texture(texBlend[i], VAR._texCoord);
#if (CURRENT_TEXTURE_COUNT % 4) == 1
        colour += getFinalColour1(blendMap, i, diffuseScale[i]);
#elif (CURRENT_TEXTURE_COUNT % 4) == 2
        colour += getFinalColour2(blendMap, i, diffuseScale[i]);
#elif (CURRENT_TEXTURE_COUNT % 4) == 3
        colour += getFinalColour3(blendMap, i, diffuseScale[i]);
#else//(CURRENT_TEXTURE_COUNT % 4) == 0
        colour += getFinalColour4(blendMap, i, diffuseScale[i]);
#endif
    }

    return colour;
}

vec3 getTerrainNormal() {

    vec3 V = VAR._vertexW.xyz - dvd_cameraPosition.xyz;

    vec3 tbn = vec3(0.0);
    vec3 tbnTemp;
    for (uint i = 0; i < MAX_TEXTURE_LAYERS; i++) {
        vec4 blendMap = texture(texBlend[i], VAR._texCoord);
#if (CURRENT_TEXTURE_COUNT % 4) == 1
        tbnTemp = getFinalTBN1(blendMap, 0, detailScale[i]);
#elif (CURRENT_TEXTURE_COUNT % 4) == 2
        tbnTemp = getFinalTBN2(blendMap, 0, detailScale[i]);
#elif (CURRENT_TEXTURE_COUNT % 4) == 3
        tbnTemp = getFinalTBN3(blendMap, 0, detailScale[i]);
#else//(CURRENT_TEXTURE_COUNT % 4) == 0
        tbnTemp = getFinalTBN4(blendMap, 0, detailScale[i]);
#endif
        tbnTemp = perturb_normal(tbnTemp, VAR._normalWV, V, VAR._texCoord);
        tbn = normalUDNBlend(tbnTemp, tbn);
    }

    return tbn;
}

#endif //_TERRAIN_SPLATTING_FRAG_