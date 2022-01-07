#ifndef _BRDF_FRAG_
#define _BRDF_FRAG_

#include "materialData.frag"
#if defined(MAIN_DISPLAY_PASS)
#include "debug.frag"
#else //MAIN_DISPLAY_PASS
#include "lightingCalc.frag"
#endif //MAIN_DISPLAY_PASS

//https://iquilezles.org/www/articles/fog/fog.htm
#if defined(NO_FOG)
#define ApplyFog(R) (R)
#else //NO_FOG
vec3 ApplyFog(in vec3 rgb) // original color of the pixel
{
    if (dvd_fogEnabled) {
        const float distance = distance(VAR._vertexW.xyz, dvd_cameraPosition.xyz); // camera to point distance
        const vec3 rayOri = dvd_cameraPosition.xyz;                                // camera position
        const vec3 rayDir = normalize(VAR._vertexW.xyz - dvd_cameraPosition.xyz);  // camera to point vector

        const float c = dvd_fogDetails._colourSunScatter.a;
        const float b = dvd_fogDetails._colourAndDensity.a;
        const float fogAmount = c * exp(-rayOri.y * b) * (1.f - exp(-distance * rayDir.y * b)) / (rayDir.y + M_EPSILON);
        return mix(rgb, dvd_fogDetails._colourAndDensity.rgb, fogAmount);
    }

    return rgb;
}
#endif //NO_FOG

vec4 getPixelColour(in vec4 albedo, in NodeMaterialData materialData, in vec3 normalWV, in float normalVariation, in vec2 uv) {
    const vec3 viewVec = normalize(VAR._viewDirectionWV);
    const float NdotV = max(dot(normalWV, viewVec), 0.f);
    const PBRMaterial material = initMaterialProperties(materialData, albedo.rgb, uv, normalWV, normalVariation, NdotV);

    vec3 radianceOut = vec3(0.f);
#if defined(MAIN_DISPLAY_PASS)
    if (getDebugColour(material, materialData, uv, normalWV, radianceOut)) {
        return vec4(radianceOut, albedo.a);
    }
#endif //MAIN_DISPLAY_PASS
    radianceOut = ApplyIBL(material, viewVec, normalWV, NdotV, VAR._vertexW.xyz, dvd_probeIndex(materialData));
    radianceOut = ApplySSR(material, radianceOut);
    radianceOut = getLightContribution(material, normalWV, viewVec, dvd_receivesShadows(materialData), radianceOut);
    radianceOut = ApplyFog(radianceOut);
    radianceOut = ApplySSAO(radianceOut);

    return vec4(radianceOut, albedo.a);
}

#endif //_BRDF_FRAG_
