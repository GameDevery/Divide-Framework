#ifndef _LIGHTING_DEFAULTS_FRAG_
#define _LIGHTING_DEFAULTS_FRAG_

#include "nodeBufferedInput.cmn"

uniform float projectedTextureMixWeight;
layout(binding = TEXTURE_PROJECTION) uniform sampler2D texDiffuseProjected;

#define PRECISION 0.000001

float DIST_TO_ZERO(float val) {
    return 1.0 - (step(-PRECISION, val) * (1.0 - step(PRECISION, val)));
}

float saturate(float v) { return clamp(v, 0.0, 1.0); }
vec3  saturate(vec3 v)  { return clamp(v, 0.0, 1.0); }
vec4  saturate(vec4 v)  { return clamp(v, 0.0, 1.0); }

void projectTexture(in vec3 PoxPosInMap, inout vec4 targetTexture){
    vec4 projectedTex = texture(texDiffuseProjected, vec2(PoxPosInMap.s, 1.0-PoxPosInMap.t));
    targetTexture.xyz = mix(targetTexture.xyz, projectedTex.xyz, projectedTextureMixWeight);
}

vec3 applyFogColor(in vec3 color){
    const float LOG2 = 1.442695;
    float zDepth = gl_FragCoord.z / gl_FragCoord.w;
    return mix(dvd_fogColor, color, clamp(exp2(-dvd_fogDensity * dvd_fogDensity * zDepth * zDepth * LOG2), 0.0, 1.0));
}

vec4 applyFog(in vec4 color) { 
    return vec4(mix(applyFogColor(color.rgb), color.rgb, dvd_fogDensity), color.a);
}

float ToLinearDepth(in float depthIn) {
#if defined(USE_SCENE_ZPLANES)
    float n = dvd_ZPlanesCombined.z;
    float f = dvd_ZPlanesCombined.w * 0.5;
#else
    float n = dvd_ZPlanesCombined.x;
    float f = dvd_ZPlanesCombined.y * 0.5;
#endif

    return (2 * n) / (f + n - (depthIn) * (f - n));
}

float ToLinearDepth(in float depthIn, in mat4 projMatrix) {
    return projMatrix[3][2] / (depthIn - projMatrix[2][2]);
}

bool InRangeExclusive(in float value, in float min, in float max) {
    return value > min && value < max;
}

vec4 positionFromDepth(in float depth,
                       in mat4 invProjectionMatrix,
                       in vec2 uv) {

    vec4 pos = vec4(2.0 * uv.x - 1.0,
                    2.0 * uv.y - 1.0,
                    2.0 * depth - 1.0,
                    1.0);

    pos = invProjectionMatrix * pos;
    pos /= pos.w;

    return pos;
}

float luminance(in vec3 rgb) {
    const vec3 kLum = vec3(0.299, 0.587, 0.114);
    return max(dot(rgb, kLum), 0.0001); // prevent zero result
}

const float gamma = 2.2;
const float invGamma = 1.0 / gamma;
const vec3 invGammaVec = vec3(invGamma);
const vec3 gammaVec = vec3(gamma);

float ToLinear(float v) { return pow(v, gamma); }
vec3  ToLinear(vec3 v)  { return pow(v, gammaVec); }
vec4  ToLinear(vec4 v)  { return vec4(pow(v.rgb, gammaVec), v.a); }

float ToSRGB(float v) { return pow(v, invGamma); }
vec3  ToSRGB(vec3 v)  { return pow(v, invGammaVec); }
vec4  ToSRGB(vec4 v)  { return vec4(pow(v.rgb, invGammaVec), v.a);}


float Gloss(vec3 bump)
{
    float gloss = 1.0;

    /*if (do_toksvig)
    {
        // Compute the "Toksvig Factor"
        float rlen = 1.0 / saturate(length(bump));
        gloss = 1.0 / (1.0 + power*(rlen - 1.0));
    }

    if (do_toksmap)
    {
        // Fetch pre-computed "Toksvig Factor"
        // and adjust for specified power
        gloss = texture2D(gloss_map, texcoord).r;
        gloss /= mix(power / baked_power, 1.0, gloss);
    }
    */
    return gloss;
}

#endif //_LIGHTING_DEFAULTS_FRAG_