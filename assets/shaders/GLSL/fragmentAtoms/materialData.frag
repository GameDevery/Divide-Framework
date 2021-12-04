#ifndef _MATERIAL_DATA_FRAG_
#define _MATERIAL_DATA_FRAG_

#include "utility.frag"
#include "texturing.frag"

#include "pbr.frag"
#include "specGloss.frag"
#include "specialBRDFs.frag"

vec3 GetBRDF(in vec3 L,
             in vec3 V,
             in vec3 N,
             in vec3 lightColour,
             in float lightAttenuation,
             in float NdotL,
             in float NdotV,
             in PBRMaterial material) 
{
    if (material._shadingMode == SHADING_OREN_NAYAR || material._shadingMode == SHADING_COOK_TORRANCE) {
        return GetBRDF_PBR(L, V, N, lightColour, lightAttenuation, NdotL, NdotV, material);
    } else if (material._shadingMode == SHADING_BLINN_PHONG || material._shadingMode == SHADING_PHONG) {
        return GetBRDF_Phong(L, V, N, lightColour, lightAttenuation, NdotL, NdotV, material);
    }

    return GetBRDF_Special(L, V, N, lightColour, lightAttenuation, NdotL, NdotV, material);
}
#if defined(COMPUTE_TBN)
#include "bumpMapping.frag"
#endif //COMPUTE_TBN

#if defined(USE_CUSTOM_TEXTURE_OMR)
void getTextureOMR(in bool usePacked, in vec3 uv, in uvec3 texOps, inout vec3 OMR);
#else //USE_CUSTOM_TEXTURE_OMR
#if defined(NO_OMR_TEX)
#define getTextureOMR(usePacked, uv, texOps, OMR)
#else //NO_OMR_TEX
void getTextureOMR(in bool usePacked, in vec3 uv, in uvec3 texOps, inout vec3 OMR) {
    if (usePacked) {
#if !defined(NO_METALNESS_TEX)
        OMR = texture(texMetalness, uv).rgb;
#endif //NO_METALNESS_TEX
    } else {
        if (texOps.x != TEX_NONE) {
#if !defined(NO_OCCLUSION_TEX)
            OMR.r = texture(texOcclusion, uv).r;
#endif //NO_METALNESS_TEX
        }
        if (texOps.y != TEX_NONE) {
#if !defined(NO_METALNESS_TEX)
            OMR.g = texture(texMetalness, uv).r;
#endif //NO_METALNESS_TEX
        }
        if (texOps.z != TEX_NONE) {
#if !defined(NO_ROUGHNESS_TEX)
            OMR.b = texture(texRoughness, uv).r;
#endif //NO_ROUGHNESS_TEX
        }
    }
}
#endif //NO_OMR_TEX
#endif //USE_CUSTOM_TEXTURE_OMR

#if defined(USE_CUSTOM_SPECULAR)
vec4 getSpecular(in NodeMaterialData matData, in vec3 uv);
#else //USE_CUSTOM_SPECULAR
vec4 getSpecular(in NodeMaterialData matData, in vec3 uv) {
    vec4 specData = vec4(Specular(matData), SpecularStrength(matData));

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

    return EmissiveColour(matData);
}
#endif //USE_CUSTOM_EMISSIVE

// A Fresnel term that dampens rough specular reflections.
// https://seblagarde.wordpress.com/2011/08/17/hello-world/
vec3 computeFresnelSchlickRoughness(in vec3 H, in vec3 V, in vec3 F0, in float roughness) {
    const float cosTheta = saturate(dot(H, V));
    return F0 + (max(vec3(1.f - roughness), F0) - F0) * pow(1.f - cosTheta, 5.f);
}

PBRMaterial initMaterialProperties(in NodeMaterialData matData, in vec3 albedo, in vec2 uv, in vec3 V, in vec3 N, in float normalVariation) {
    PBRMaterial material;

    const vec4 unpackedData = (unpackUnorm4x8(matData._data.z) * 255);

    material._shadingMode = uint(unpackedData.y);

    vec4 OMR_Selection = unpackUnorm4x8(matData._data.x);
    {
        const bool usePacked = uint(unpackedData.z) == 1u;
        const uvec4 texOpsB = dvd_texOperationsB(matData);
        getTextureOMR(usePacked, vec3(uv, 0), texOpsB.xyz, OMR_Selection.rgb);

        material._occlusion = OMR_Selection.r;
        material._metallic = OMR_Selection.g;
        material._roughness = OMR_Selection.b;
    }

    material._emissive = getEmissiveColour(matData, vec3(uv, 0));
    { //selection
        const uint selection = uint(OMR_Selection.a * 2);
        material._emissive += vec3(0.f, selection == 1u ? 2.f : 0.f, selection == 2u ? 2.f : 0.f);
    }

    material._specular = getSpecular(matData, vec3(uv, 0));

    // Deduce a roughness factor from specular colour and shininess
    if (material._shadingMode != SHADING_OREN_NAYAR && material._shadingMode != SHADING_COOK_TORRANCE) {
        const float specularIntensity = Luminance(material._specular.rgb);
        const float specularPower = material._specular.a / 1000.f;
        const float roughnessFactor = 1.f - sqrt(specularPower);
        // Specular intensity directly impacts roughness regardless of shininess
        material._roughness = (1.f - (saturate(pow(roughnessFactor, 2)) * specularIntensity));
    }

    // Try to reduce specular aliasing by increasing roughness when minified normal maps have high variation.
    material._roughness = mix(material._roughness, 1.f, normalVariation);

    const vec3 albedoIn = albedo + Ambient(matData);
    const vec3 dielectricSpecular = vec3(0.04f);
    const vec3 black = vec3(0.f);
    material._diffuseColour = mix(albedoIn * (vec3(1.f) - dielectricSpecular), black, material._metallic);
    material._F0 = mix(dielectricSpecular, albedoIn, material._metallic);
    material._a = max(SQUARED(material._roughness), 0.01f);

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
    return saturate(a + kernelRoughness2);
}
#endif //!PRE_PASS

#if !defined(PRE_PASS)
#if defined(USE_PLANAR_REFLECTION)
#define getReflectionColour(UV) texture(texReflectPlanar, UV)
#else //USE_PLANAR_REFLECTION
#define getReflectionColour(UV) texture(texReflectCube, UV)
#endif //USE_PLANAR_REFLECTION

#if defined(USE_PLANAR_REFRACTION)
#define getRefractionColour(UV) texture(texRefractPlanar, UV)
#else //USE_PLANAR_REFLECTION
#define getRefractionColour(UV) texture(texRefractCube, UV)
#endif //USE_PLANAR_REFLECTION
#endif //!PRE_PASS

vec4 getTextureColour(in NodeMaterialData data, in vec3 uv) {
    vec4 colour = BaseColour(data);

    const uvec2 texOps = dvd_texOperationsA(data).xy;

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
        const float refAlpha = dvd_useOpacityAlphaChannel(data) ? texture(texOpacityMap, uv).a : texture(texOpacityMap, uv).r;
        return getScaledAlpha(refAlpha, uv.xy, textureSize(texOpacityMap, 0));
    }

    if (dvd_useAlbedoTextureAlphaChannel(data) && dvd_TexOpUnit0(data) != TEX_NONE) {
        return getAlpha(texDiffuse0, uv);
    }

    return BaseColour(data).a;
}
#endif //USE_ALPHA_DISCARD

vec4 getAlbedo(in NodeMaterialData data, in vec3 uv) {
    vec4 albedo = getTextureColour(data, uv);

    if (dvd_TexOpOpacity(data) != TEX_NONE) {
        const float refAlpha = dvd_useOpacityAlphaChannel(data) ? texture(texOpacityMap, uv).a : texture(texOpacityMap, uv).r;
        albedo.a = getScaledAlpha(refAlpha, uv.xy, textureSize(texOpacityMap, 0));
    }

    if (!dvd_useAlbedoTextureAlphaChannel(data)) {
        albedo.a = BaseColour(data).a;
    } else if (dvd_TexOpUnit0(data) != TEX_NONE) {
        albedo.a = getScaledAlpha(albedo.a, uv.xy, textureSize(texDiffuse0, 0));
    }

    return albedo;
}
#else //HAS_TRANSPARENCY
#define getAlbedo getTextureColour
#endif //HAS_TRANSPARENCY

vec4 getNormalMapAndVariation(in sampler2DArray tex, in vec3 uv) {
    const vec3 normalMap = 2.f * texture(tex, uv).rgb - 1.f;
    const float normalMap_Mip = textureQueryLod(tex, uv.xy).x;
    const float normalMap_Length = length(normalMap);
    const float variation = 1.f - pow(normalMap_Length, 8.f);
    const float minification = saturate(normalMap_Mip - 2.f);

    const float normalVariation = variation * minification;
    const vec3 normalW = (normalMap / normalMap_Length);

    return vec4(normalW, normalVariation);
}

vec3 getNormalWV(in NodeMaterialData data, in vec3 uv, out float normalVariation) {
    normalVariation = 0.f;

    vec3 normalWV = VAR._normalWV;
#if defined(COMPUTE_TBN) && !defined(USE_CUSTOM_NORMAL_MAP)
    if (dvd_bumpMethod(MATERIAL_IDX) != BUMP_NONE) {
        const vec4 normalData = getNormalMapAndVariation(texNormalMap, uv);
        normalWV = getTBNWV() * normalData.xyz;
        normalVariation = normalData.w;
    }
#endif //COMPUTE_TBN && !USE_CUSTOM_NORMAL_MAP

    return normalize(normalWV) * 
        (dvd_isDoubleSided(data) ? (2.f * float(gl_FrontFacing) - 1.f)
                                 : 1.f);
}

#endif //_MATERIAL_DATA_FRAG_
