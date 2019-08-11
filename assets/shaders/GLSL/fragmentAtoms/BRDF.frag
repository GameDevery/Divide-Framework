#ifndef _BRDF_FRAG_
#define _BRDF_FRAG_

#include "lightInput.cmn"

#include "lightData.frag"
#include "materialData.frag"
#include "shadowMapping.frag"

#if defined(USE_SHADING_COOK_TORRANCE) || defined(USE_SHADING_OREN_NAYAR)
#include "pbr.frag"
vec4 getExtraData(in mat4 colourMatrix, in vec2 uv) {
    return vec4(getMetallicRoughness(colourMatrix, uv), getRimLighting(colourMatrix, uv), 0.0f);
}
#define getBRDFFactors(LDir, LCol, MetallicRoughness, Albedo, N, V) PBR(LDir, LCol, MetallicRoughness, Albedo, N, V);
#elif defined(USE_SHADING_PHONG) || defined (USE_SHADING_BLINN_PHONG)
#include "phong_lighting.frag"
vec4 getExtraData(in mat4 colourMatrix, in vec2 uv) {
    return vec4(getSpecular(colourMatrix, uv), getShininess(colourMatrix));
}
#define getBRDFFactors(LDir, LCol, SpecularShininess, Albedo, N, V) Phong(normalize(LDir), LCol, SpecularShininess, Albedo, N, V)
#else
#if defined(USE_SHADING_TOON) // ToDo
#   define getBRDFFactors(LDir, LCol, Spec, Albedo, N, V) vec4(0.6f, 0.2f, 0.9f, 0.0f); //obvious pink
#else
#   define getBRDFFactors(LDir, LCol, Spec, Albedo, N, V) vec4(0.6f, 1.0f, 0.7f, 0.0f); //obvious lime-green
#endif
#endif

#if !defined(MAX_LIGHTS_PER_PASS)
#define MAX_LIGHTS_PER_PASS MAX_NUM_LIGHTS_PER_TILE
#endif

void getDirectionalLightContribution(in vec3 albedo, in vec4 data, in vec3 normalWV, in vec3 viewDir, inout vec4 lightColour) {
    const uint dirLightCount = dvd_LightData.x;

    for (uint lightIdx = 0; lightIdx < dirLightCount; ++lightIdx) {
        const Light light = dvd_LightSource[lightIdx];
        vec4 ret = getBRDFFactors(-light._directionWV.xyz, vec4(light._colour.rgb, 1.0f), data, vec4(albedo, getShadowFactor(light._options.y)), normalWV, viewDir);

        lightColour.rgb += ret.rgb;
        lightColour.a = max(ret.a, lightColour.a);
    }
}

void getPointLightContribution(in uint tileIndex, in vec3 albedo, in vec4 data, in vec3 normalWV, in vec3 viewDir, inout vec4 lightColour) {
    const uint dirLightCount = dvd_LightData.x;
    const uint tileOffset = (tileIndex * MAX_NUM_LIGHTS_PER_TILE);

    for (uint i = 0; i < MAX_LIGHTS_PER_PASS; ++i) {
        const int lightIdx = perTileLightIndices[tileOffset + i];
        if (lightIdx == -1) {
            break;
        }

        const Light light = dvd_LightSource[lightIdx + dirLightCount];

        vec3 lightDirection = VAR._vertexWV.xyz - light._positionWV.xyz;

        const vec4 colourAndAtt = vec4(light._colour.rgb, getLightAttenuationPoint(light, lightDirection));

        vec4 ret = getBRDFFactors(lightDirection, colourAndAtt, data, vec4(albedo, getShadowFactor(light._options.y)), normalWV, viewDir);

        lightColour.rgb += ret.rgb;
        lightColour.a = max(ret.a, lightColour.a);
    }
}

void getSpotLightContribution(in uint tileIndex, in vec3 albedo, in vec4 data, in vec3 normalWV, in vec3 viewDir, inout vec4 lightColour) {
    const uint dirLightCount = dvd_LightData.x;
    const uint pointLightCount = dvd_LightData.y;
    const uint tileOffset = (tileIndex * MAX_NUM_LIGHTS_PER_TILE) + pointLightCount;

    for (uint i = 0; i < MAX_LIGHTS_PER_PASS; ++i) {
        const int lightIdx = perTileLightIndices[tileOffset + i];
        if (lightIdx == -1) {
            break;
        }

        const Light light = dvd_LightSource[lightIdx + dirLightCount];
        vec3 lightDirection = VAR._vertexWV.xyz - light._positionWV.xyz;
        const vec4 colourAndAtt = vec4(light._colour.rgb, getLightAttenuationSpot(light, lightDirection));

        vec4 ret = getBRDFFactors(lightDirection, colourAndAtt, data, vec4(albedo, getShadowFactor(light._options.y)), normalWV, viewDir);

        lightColour.rgb += ret.rgb;
        lightColour.a = max(ret.a, lightColour.a);
    }
}

vec3 getLitColour(in vec3 albedo, in mat4 colourMatrix, in vec3 normalWV, in vec2 uv) {
    const uint tileIndex = GetTileIndex(gl_FragCoord.xy);

#if defined(USE_SHADING_FLAT)
    return albedo;
#else //USE_SHADING_FLAT

    const vec3 viewDirNorm = normalize(-VAR._vertexWV.xyz);
    const vec4 data = getExtraData(colourMatrix, uv);

    // Apply all lighting contributions (.a = reflectionCoeff)
    vec4 lightColour = vec4(0.0f);
    getDirectionalLightContribution(albedo, data, normalWV, viewDirNorm, lightColour);

    if (dvd_lodLevel < 2)
    {
         const uint tileIndex = GetTileIndex(gl_FragCoord.xy);
         getPointLightContribution(tileIndex, albedo, data, normalWV, viewDirNorm, lightColour);
         // Move past the first sentinel to get to the spot lights
         getSpotLightContribution(tileIndex, albedo, data, normalWV, viewDirNorm, lightColour);
    }
    
    lightColour.rgb += getEmissive(colourMatrix);

    /*const vec3 ambient = vec3(0.01f, 0.01f, 0.01f);
    lightColour.rgb += (ambient * when_lt(lightColour.a, 0.01f));

    // Specular reflections
    if (lightColour.a > 0.0f) {
        if (dvd_lodLevel < 1 && getReflectivity(colourMatrix) > 100) {
            vec3 reflectDirection = reflect(viewDirNorm, normalWV);
            reflectDirection = vec3(inverse(dvd_ViewMatrix) * vec4(reflectDirection, 0.0f));
            //lightColour.rgb = mix(texture(texEnvironmentCube, vec4(reflectDirection, dvd_reflectionIndex)).rgb,
            //                  lightColour.rgb,
            //                  vec3(saturate(lightColour.a)));
        }
    }
    */
    return lightColour.rgb;
#endif //USE_SHADING_FLAT
}

vec4 getPixelColour(in vec4 albedo, in mat4 colourMatrix, in vec3 normalWV, in vec2 uv) {
    vec4 colour = vec4(getLitColour(albedo.rgb, colourMatrix, normalWV, uv), albedo.a);

#if !defined(DISABLE_SHADOW_MAPPING) && defined(DEBUG_SHADOWMAPPING)
    if (dvd_showDebugInfo) {
        switch (getShadowData()) {
            case -1: colour.rgb = vec3(1.0f); break;
            case  0: colour.r += 0.15f; break;
            case  1: colour.g += 0.25f; break;
            case  2: colour.b += 0.40f; break;
            case  3: colour.rgb += vec3(0.15f, 0.25f, 0.40f); break;
        };
    }
#endif //DEBUG_SHADOWMAPPING

    return colour;
}

#endif