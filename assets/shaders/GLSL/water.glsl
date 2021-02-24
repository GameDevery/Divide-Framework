-- Vertex

#include "vbInputData.vert"
#include "lightingDefaults.vert"

layout(location = 0) out flat int _underwater;
layout(location = 1) out vec4 _vertexWVP;

void main(void)
{
    const NodeTransformData data = fetchInputData();
    _vertexWVP = computeData(data);
    setClipPlanes();
    computeLightVectors(data);

    _underwater = dvd_cameraPosition.y < VAR._vertexW.y ? 1 : 0;

    gl_Position = _vertexWVP;
}


-- Fragment

layout(location = 0) in flat int _underwater;
layout(location = 1) in vec4 _vertexWVP;

uniform vec3 _refractionTint;
uniform float _specularShininess;
uniform vec2 _noiseTile;
uniform vec2 _noiseFactor;

#define NO_ENV_MAPPING
#define NO_SSAO
#define USE_SHADING_BLINN_PHONG
#define SHADOW_INTENSITY_FACTOR 0.5f

#include "environment.frag"
#include "BRDF.frag"
#include "output.frag"

const float Eta = 0.15f; //water

float Fresnel(in vec3 viewDir, in vec3 normal) {
    const float fresnel = Eta + (1.0f - Eta) * pow(max(0.0f, 1.0f - dot(viewDir, normal)), 5.0f);
    return saturate(fresnel);
}

void main()
{
    const float time2 = MSToSeconds(dvd_time) * 0.05f;
    const vec2 uvNormal0 = (VAR._texCoord * _noiseTile) + vec2( time2, time2);
    const vec2 uvNormal1 = (VAR._texCoord * _noiseTile) + vec2(-time2, time2);

    const vec4 normalData0 = getNormalMapAndVariation(texNormalMap, uvNormal0);
    const vec4 normalData1 = getNormalMapAndVariation(texNormalMap, uvNormal1);

    const vec3 normal0 = normalData0.xyz;
    const vec3 normal1 = normalData1.xyz;
    const float normalVariation = max(normalData0.w, normalData1.w);

    const vec3 normalWV = normalize(getTBNWV() * ((normal0 + normal1) * 0.5f));
    const vec3 normalW = normalize(mat3(dvd_InverseViewMatrix) * normalWV);

    //vec3 normal = normalPartialDerivativesBlend(normal0, normal1);
    //const vec3 normalWV = normalize(mat3(dvd_ViewMatrix) * normalW);
    const vec3 incident = normalize(dvd_cameraPosition.xyz - VAR._vertexW.xyz);

    const vec2 waterUV = clamp(0.5f * homogenize(_vertexWVP).xy + 0.5f, vec2(0.001f), vec2(0.999f));
    const vec3 refractionColour = overlayVec(texture(texRefract, waterUV).rgb, _refractionTint);

    switch (dvd_materialDebugFlag) {
        case DEBUG_REFRACTIONS:
            writeScreenColour(vec4(refractionColour, 1.f), normalWV);
            return;
        case DEBUG_REFLECTIONS:
            writeScreenColour(texture(texReflect, waterUV), normalWV);
            return;
    }

    // Get a modified viewing direction of the camera that only takes into account height.
    // ref: https://www.rastertek.com/terdx10tut16.html
    const vec3 texColour = mix(mix(refractionColour, 
                                   texture(texReflect, waterUV).rgb,
                                   Fresnel(incident, normalW)),
                               refractionColour,
                               _underwater);

    NodeMaterialData data = dvd_Materials[MATERIAL_IDX];

    vec3 MetalnessRoughnessProbeID = vec3(0.f, 1.f, 0.f);
    vec3 SpecularColourOut = vec3(0.f);
    const vec4 outColour = getPixelColour(vec4(texColour, 1.0f), data, normalWV, normalVariation, VAR._texCoord, SpecularColourOut, MetalnessRoughnessProbeID);

    // Guess work based on what "look right"
    const float lerpValue = saturate(2.95f * (dvd_sunDirection.y + 0.15f));
    const vec3 lightDirection = mix(-dvd_sunDirection.xyz, dvd_sunDirection.xyz, lerpValue);

    // Calculate the reflection vector using the normal and the direction of the light.
    const vec3 reflection = -reflect(lightDirection, normalW);
    // Calculate the specular light based on the reflection and the camera position.
    const float specular = dot(normalize(reflection), incident);
    // Increase the specular light by the shininess value and add the specular to the final color.
    // Check to make sure the specular was positive so we aren't adding black spots to the water.
    writeScreenColour(outColour + (specular > 0.f ? pow(specular, _specularShininess) : 0.f), normalWV, SpecularColourOut, MetalnessRoughnessProbeID);
}
