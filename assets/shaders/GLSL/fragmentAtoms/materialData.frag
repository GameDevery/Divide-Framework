#ifndef _MATERIAL_DATA_FRAG_
#define _MATERIAL_DATA_FRAG_

#if defined(SHADING_MODE_PBR_MR) || defined(SHADING_MODE_PBR_SG)
#define SHADING_MODE_PBR
#endif //SHADING_MODE_PBR_MR || SHADING_MODE_PBR_SG
#if defined(NO_POST_FX)
#if !defined(NO_FOG)
#define NO_FOG
#endif //!NO_FOG
#endif //NO_POST_FX

struct PBRMaterial
{
    vec4  _specular;
    vec3  _diffuseColour;
    vec3 _F0;
    vec3  _emissive;
    float _occlusion;
    float _metallic;
    float _roughness;
#if defined(SHADING_MODE_PBR)
    float _a;
#endif //SHADING_MODE_PBR
};

#if !defined(MAIN_DISPLAY_PASS)

#if !defined(NO_SSAO)
#define NO_SSAO
#endif //!NO_SSAO

#if !defined(NO_FOG)
#define NO_FOG
#endif //!NO_FOG
#else//!MAIN_DISPLAY_PASS
#if !defined(PRE_PASS)
#if defined(MSAA_SCREEN_TARGET)
layout(binding = TEXTURE_SCENE_NORMALS) uniform sampler2DMS texSceneNormals;
#else//MSAA_SCREEN_TARGET
layout(binding = TEXTURE_SCENE_NORMALS) uniform sampler2D texSceneNormals;
#endif //MSAA_SCREEN_TARGET
#endif //PRE_PASS
#endif //!MAIN_DISPLAY_PASS

#include "utility.frag"
#include "texturing.frag"

#if defined(COMPUTE_TBN)
#include "bumpMapping.frag"
#endif //COMPUTE_TBN

#if !defined(NO_SSAO)
layout(binding = TEXTURE_SSAO_SAMPLE) uniform sampler2D texSSAO;
#endif //NO_SSAO

#if !defined(PRE_PASS)

#if defined(MSAA_SCREEN_TARGET)
#define sampleTexSceneNormals() texelFetch(texSceneNormals, ivec2(gl_FragCoord.xy), gl_SampleID)
#else  //MSAA_SCREEN_TARGET
#define sampleTexSceneNormals() texture(texSceneNormals, dvd_screenPositionNormalised)
#endif //MSAA_SCREEN_TARGET
#endif //!PRE_PASS

#if defined(USE_CUSTOM_TEXTURE_OMR)
void getTextureOMR(in bool usePacked, in vec3 uv, in uvec3 texOps, inout vec3 OMR);
#else //USE_CUSTOM_TEXTURE_OMR
#if defined(NO_OMR_TEX)
#define getTextureOMR(usePacked, uv, texOps, OMR)
#else //NO_OMR_TEX
void getTextureOMR(in bool usePacked, in vec3 uv, in uvec3 texOps, inout vec3 OMR) {
    if (usePacked) {
        OMR = texture(texMetalness, uv).rgb;
    } else {
#if !defined(NO_OCCLUSION_TEX)
        if (texOps.x != TEX_NONE) {
            OMR.r = texture(texOcclusion, uv).r;
        }
#endif //NO_METALNESS_TEX
#if !defined(NO_METALNESS_TEX)
        if (texOps.y != TEX_NONE) {
            OMR.g = texture(texMetalness, uv).r;
        }
#endif //NO_METALNESS_TEX
#if !defined(NO_ROUGHNESS_TEX)
        if (texOps.z != TEX_NONE) {
            OMR.b = texture(texRoughness, uv).r;
        }
#endif //NO_ROUGHNESS_TEX
    }
}
#endif //NO_OMR_TEX
#endif //USE_CUSTOM_TEXTURE_OMR

vec4 ApplyTexOperation(in vec4 a, in vec4 b, in uint texOperation) {
    //hot pink to easily spot it in a crowd
    vec4 retColour = a;

    // Read from the second texture (if any)
    switch (texOperation) {
        
        default             : retColour = vec4(0.7743f, 0.3188f, 0.5465f, 1.f);               break;
        case TEX_NONE       : /*NOP*/                                                         break;
        case TEX_MULTIPLY   : retColour *= b;                                                 break;
        case TEX_ADD        : retColour.rgb += b.rgb; retColour.a *= b.a;                     break;
        case TEX_SUBTRACT   : retColour -= b;                                                 break;
        case TEX_DIVIDE     : retColour /= b;                                                 break;
        case TEX_SMOOTH_ADD : retColour = (retColour + b) - (retColour * b);                  break;
        case TEX_SIGNED_ADD : retColour += b - 0.5f;                                          break;
        case TEX_DECAL      : retColour  = vec4(mix(retColour.rgb, b.rgb, b.a), retColour.a); break;
        case TEX_REPLACE    : retColour  = b;                                                 break;
    }

    return retColour;
}

#if defined(USE_CUSTOM_SPECULAR)
vec4 getSpecular(in NodeMaterialData matData, in vec3 uv);
#else //USE_CUSTOM_SPECULAR
vec4 getSpecular(in NodeMaterialData matData, in vec3 uv) {
    vec4 specData = vec4(dvd_Specular(matData), dvd_SpecularStrength(matData));

    // Specular strength is baked into the specular colour, so even if we use a texture, we still need to multiply the strength in
    const uint texOp = dvd_TexOpSpecular(matData);
    if (texOp != TEX_NONE) {
        specData.rgb = ApplyTexOperation(vec4(specData.rgb, 1.f),
                                         texture(texSpecular, uv),
                                         texOp).rgb;
    }

    return specData;
}
#endif //USE_CUSTOM_SPECULAR

#if defined(USE_CUSTOM_EMISSIVE)
vec3 getEmissiveColour(in NodeMaterialData matData, in vec3 uv);
#else //USE_CUSTOM_EMISSIVE
vec3 getEmissiveColour(in NodeMaterialData matData, in vec3 uv) {
    const uint texOp = dvd_TexOpEmissive(matData);
    if (texOp != TEX_NONE) {
        return texture(texEmissive, uv).rgb;
    }

    return dvd_EmissiveColour(matData);
}
#endif //USE_CUSTOM_EMISSIVE

float SpecularToMetalness(in vec3 specular, in float power) {
    return 0.f; //sqrt(power / MAX_SHININESS);
}

float SpecularToRoughness(in vec3 specular, in float power) {
    const float roughnessFactor = 1.f - sqrt(power / MAX_SHININESS);
    // Specular intensity directly impacts roughness regardless of shininess
    return 1.f - ((Saturate(pow(roughnessFactor, 2)) * Luminance(specular)));
}

float getRoughness(in NodeMaterialData matData, in vec2 uv, in float normalVariation) {
    float roughness = 0.f;

    const vec4 unpackedData = (unpackUnorm4x8(matData._data.z) * 255);
    const bool usePacked = uint(unpackedData.z) == 1u;

    vec4 OMR = unpackUnorm4x8(matData._data.x);
    const uvec4 texOpsB = dvd_TexOperationsB(matData);
    getTextureOMR(usePacked, vec3(uv, 0), texOpsB.xyz, OMR.rgb);
    roughness = OMR.b;

#if defined(SHADING_MODE_BLINN_PHONG)
    // Deduce a roughness factor from specular colour and shininess
    const vec4 specular = getSpecular(matData, vec3(uv, 0));
    roughness = SpecularToRoughness(specular.rgb, specular.a);
#endif //SHADING_MODE_BLINN_PHONG

    // Try to reduce specular aliasing by increasing roughness when minified normal maps have high variation.
    roughness = mix(roughness, 1.f, normalVariation);

    return roughness;
}

PBRMaterial initMaterialProperties(in NodeMaterialData matData, in vec3 albedo, in vec2 uv, in vec3 N, in float nDotV) {
    PBRMaterial material;
    material._emissive = getEmissiveColour(matData, vec3(uv, 0));
    material._specular = getSpecular(matData, vec3(uv, 0));

    vec4 OMR = unpackUnorm4x8(matData._data.x);

    #define UnpackedData (unpackUnorm4x8(matData._data.z) * 255)
    #define UsePacked (uint(UnpackedData.z) == 1u)

    getTextureOMR(UsePacked, vec3(uv, 0), dvd_TexOperationsB(matData).xyz, OMR.rgb);
    material._occlusion = OMR.r;
    material._metallic = OMR.g;

#if defined(MAIN_DISPLAY_PASS) && !defined(PRE_PASS)
    material._roughness = sampleTexSceneNormals().b;
#else //MAIN_DISPLAY_PASS && !PRE_PASS
    material._roughness = OMR.b;
#if defined(SHADING_MODE_BLINN_PHONG)
    material._roughness = SpecularToRoughness(material._specular.rgb, material._specular.a);
#endif //SHADING_MODE_BLINN_PHONG
#endif //MAIN_DISPLAY_PASS && !PRE_PASS

#if defined(SHADING_MODE_BLINN_PHONG)
    material._metallic = SpecularToMetalness(material._specular.rgb, material._specular.a);
#endif //SHADING_MODE_BLINN_PHONG
    const vec3 albedoIn = albedo + dvd_Ambient(matData);

#define DielectricSpecular vec3(0.04f)
#define Black vec3(0.f)

    material._diffuseColour = mix(albedoIn * (vec3(1.f) - DielectricSpecular), Black, material._metallic);
    material._F0 = mix(DielectricSpecular, albedoIn, material._metallic);

#undef Black
#undef DielectricSpecular

    return material;
}


#if defined(USE_CUSTOM_TBN)
mat3 getTBNWV();
#else //USE_CUSTOM_TBN
#if defined(COMPUTE_TBN)
#define getTBNWV() VAR._tbnWV
#else //COMPUTE_TBN
// Default: T - X-axis, B - Z-axis, N - Y-axis
#define getTBNWV() mat3(vec3(1.f, 0.f, 0.f), vec3(0.f, 0.f, 1.f), vec3(1.f, 0.f, 0.f))
#endif //COMPUTE_TBN
#endif //USE_CUSTOM_TBN

#if !defined(PRE_PASS)
// Reduce specular aliasing by producing a modified roughness value
// Tokuyoshi et al. 2019. Improved Geometric Specular Antialiasing.
// http://www.jp.square-enix.com/tech/library/pdf/ImprovedGeometricSpecularAA.pdf
float specularAntiAliasing(in vec3 N, in float a) {
    // normal-based isotropic filtering
    // this is originally meant for deferred rendering but is a bit simpler to implement than the forward version
    // saves us from calculating uv offsets and sampling textures for every light

     // squared std dev of pixel filter kernel (in pixels)
    #define SIGMA2 0.25f
    // clamping threshold
    #define KAPPA  0.18f

    const vec3 dndu = dFdx(N);
    const vec3 dndv = dFdy(N);
    const float variance = SIGMA2 * (dot(dndu, dndu) + dot(dndv, dndv));
    const float kernelRoughness2 = min(2.f * variance, KAPPA);
    return Saturate(a + kernelRoughness2);
}
#endif //!PRE_PASS

vec4 getTextureColour(in NodeMaterialData data, in vec3 uv) {
    vec4 colour = dvd_BaseColour(data);

    const uvec2 texOps = dvd_TexOperationsA(data).xy;

    if (texOps.x != TEX_NONE) {
        colour = ApplyTexOperation(colour, texture(texDiffuse0, uv), texOps.x);
    }
    if (texOps.y != TEX_NONE) {
        colour = ApplyTexOperation(colour, texture(texDiffuse1, uv), texOps.y);
    }

    return colour;
}

#if defined(HAS_TRANSPARENCY)

#if defined(USE_ALPHA_DISCARD)
float getAlpha(in NodeMaterialData data, in vec3 uv) {
    if (dvd_TexOpOpacity(data) != TEX_NONE) {
        const float refAlpha = dvd_UseOpacityAlphaChannel(data) ? texture(texOpacityMap, uv).a : texture(texOpacityMap, uv).r;
        return getScaledAlpha(refAlpha, uv.xy, textureSize(texOpacityMap, 0));
    }

    if (dvd_UseAlbedoTextureAlphaChannel(data) && dvd_TexOpUnit0(data) != TEX_NONE) {
        return getAlpha(texDiffuse0, uv);
    }

    return dvd_BaseColour(data).a;
}
#endif //USE_ALPHA_DISCARD

vec4 getAlbedo(in NodeMaterialData data, in vec3 uv) {
    vec4 albedo = getTextureColour(data, uv);

    if (dvd_TexOpOpacity(data) != TEX_NONE) {
        const float refAlpha = dvd_UseOpacityAlphaChannel(data) ? texture(texOpacityMap, uv).a : texture(texOpacityMap, uv).r;
        albedo.a = getScaledAlpha(refAlpha, uv.xy, textureSize(texOpacityMap, 0));
    }

    if (!dvd_UseAlbedoTextureAlphaChannel(data)) {
        albedo.a = dvd_BaseColour(data).a;
    } else if (dvd_TexOpUnit0(data) != TEX_NONE) {
        albedo.a = getScaledAlpha(albedo.a, uv.xy, textureSize(texDiffuse0, 0));
    }

    return albedo;
}
#else //HAS_TRANSPARENCY
#define getAlbedo getTextureColour
#endif //HAS_TRANSPARENCY

#if defined(MAIN_DISPLAY_PASS)
vec3 getNormalMap(in sampler2DArray tex, in vec3 uv, out float normalVariation) {
    const vec3 normalMap = 2.f * texture(tex, uv).rgb - 1.f;
    const float normalMap_Mip = textureQueryLod(tex, uv.xy).x;
    const float normalMap_Length = length(normalMap);
    const float variation = 1.f - pow(normalMap_Length, 8.f);
    const float minification = Saturate(normalMap_Mip - 2.f);

    normalVariation = variation * minification;
    const vec3 normalW = (normalMap / normalMap_Length);

    return normalW;
}
#else //MAIN_DISPLAY_PASS
vec3 getNormalMap(in sampler2DArray tex, in vec3 uv) {
    return normalize(2.f * texture(tex, uv).rgb - 1.f);
}
vec3 getNormalMap(in sampler2DArray tex, in vec3 uv, out float normalVariation) {
    normalVariation = 0.f;
    return normalize(2.f * texture(tex, uv).rgb - 1.f);
}
#endif //MAIN_DISPLAY_PASS

vec3 getNormalWV(in NodeMaterialData data, in vec3 uv, out float normalVariation) {
    normalVariation = 0.f;
#if defined(MAIN_DISPLAY_PASS) && !defined(PRE_PASS)
    return normalize(unpackNormal(sampleTexSceneNormals().rg));
#else //MAIN_DISPLAY_PASS && !PRE_PASS
    vec3 normalWV = VAR._normalWV;
#if defined(COMPUTE_TBN)
    if (dvd_BumpMethod(MATERIAL_IDX) != BUMP_NONE) {
        const vec3 normalData = getNormalMap(texNormalMap, uv, normalVariation);
        normalWV = getTBNWV() * normalData;
    }
#endif //COMPUTE_TBN
    return normalize(normalWV) * (dvd_IsDoubleSided(data) ? (2.f * float(gl_FrontFacing) - 1.f) : 1.f);
#endif  //MAIN_DISPLAY_PASS && !PRE_PASS
}

vec3 getNormalWV(in NodeMaterialData data, in vec3 uv) {
    float variation = 0.f;
    return getNormalWV(data, uv, variation);
}
#endif //_MATERIAL_DATA_FRAG_
