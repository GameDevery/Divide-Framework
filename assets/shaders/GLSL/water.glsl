-- Vertex

#include "vbInputData.vert"
#include "lightingDefaults.vert"

layout(location = 0) out flat int _underwater;
layout(location = 1) out vec4 _vertexWVP;

void main(void)
{
    computeData();
    
    computeLightVectors();

    _vertexWVP = dvd_ProjectionMatrix * VAR._vertexWV;
    _underwater = dvd_cameraPosition.y < VAR._vertexW.y ? 1 : 0;

    gl_Position = _vertexWVP;
}

-- Fragment

#define SHADOW_INTENSITY_FACTOR 0.5f

layout(location = 0) in flat int _underwater;
layout(location = 1) in vec4 _vertexWVP;

uniform vec2 _noiseTile;
uniform vec2 _noiseFactor;

#define USE_SHADING_BLINN_PHONG
#define USE_DEFERRED_NORMALS

#if defined(PRE_PASS)
#include "prePass.frag"
#else
#include "BRDF.frag"
#include "output.frag"

const float Eta = 0.15f; //water

float Fresnel(in vec3 viewDir, in vec3 normal) {
    return mix(Eta + (1.0 - Eta) * pow(max(0.0f, 1.0f - dot(viewDir, normal)), 5.0f), 1.0f, _underwater * 1.0f);
}
#endif

void main()
{
#if defined(PRE_PASS)
    const float kDistortion = 0.015f;
    const float kRefraction = 0.09f;

    float time2 = float(dvd_time) * 0.00001f;
    vec2 uvNormal0 = VAR._texCoord * _noiseTile;
    uvNormal0.s += time2;
    uvNormal0.t += time2;
    vec2 uvNormal1 = VAR._texCoord * _noiseTile;
    uvNormal1.s -= time2;
    uvNormal1.t += time2;

    vec3 normal0 = getBump(uvNormal0);
    vec3 normal1 = getBump(uvNormal1);

    outputWithVelocity(VAR._texCoord, 1.0f, computeDepth(VAR._vertexWV), normalize(VAR._tbn * normalize(normal0 + normal1)));
#else

    vec3 normal = getNormal(VAR._texCoord);
    vec4 uvReflection = clamp(((_vertexWVP / _vertexWVP.w) + 1.0f) * 0.5f, vec4(0.001f), vec4(0.999f));
    vec3 incident = normalize(-VAR._vertexWV.xyz);

    vec2 uvFinalReflect = uvReflection.xy + _noiseFactor * normal.xy;
    vec2 uvFinalRefract = uvReflection.xy + _noiseFactor * normal.xy;

    //vec4 distOffset = texture(texDiffuse0, VAR._texCoord + vec2(time2)) * kDistortion;
    //vec4 dudvColor = texture(texDiffuse0, vec2(VAR._texCoord + distOffset.xy));
    //dudvColor = normalize(dudvColor * 2.0 - 1.0) * kRefraction;

    //normal = texture(texNormalMap, vec2(VAR._texCoord + dudvColor.xy)).rgb;

    mat4 colourMatrix = dvd_Matrices[VAR.dvd_baseInstance]._colourMatrix;
    vec4 mixFactor = vec4(clamp(Fresnel(incident, normalize(VAR._normalWV)), 0.0f, 1.0f));
    vec4 texColour = mix(texture(texReflectPlanar, uvFinalReflect),
                         texture(texRefractPlanar, uvFinalRefract),
                         mixFactor);
    
    writeOutput(getPixelColour(vec4(texColour.rgb, 1.0f), colourMatrix, normal, VAR._texCoord));
#endif
}