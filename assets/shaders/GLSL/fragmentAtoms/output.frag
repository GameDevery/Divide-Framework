#ifndef _OUTPUT_FRAG_
#define _OUTPUT_FRAG_

#define MIN_FLOAT_OUTPUT -65504.f

#if defined(MAIN_DISPLAY_PASS)
layout(location = TARGET_NORMALS_AND_MATERIAL_DATA) out vec4 _matDataOut;
layout(location = TARGET_SPECULAR) out vec3 _specularDataOut;
#endif //MAIN_DISPLAY_PASS

#if !defined(OIT_PASS)
layout(location = TARGET_ALBEDO) out vec4 _colourOut;
#else //OIT_PASS
layout(location = TARGET_ACCUMULATION) out vec4  _accum;
layout(location = TARGET_REVEALAGE) out float _revealage;

#if defined(USE_COLOURED_WOIT)
layout(location = TARGET_MODULATE) out vec4  _modulate;
#endif

//layout(binding = TEXTURE_POST_FX_DATA) uniform sampler2D texTransmitance;
layout(binding = TEXTURE_DEPTH_MAP) uniform sampler2D texDepthMap;

// Shameless copy-paste from http://casual-effects.blogspot.co.uk/2015/03/colored-blended-order-independent.html
void writePixel(in vec4 premultipliedReflect, in vec3 transmit, in float viewSpaceZ) {
#if defined(USE_COLOURED_WOIT)
    // NEW: Perform this operation before modifying the coverage to account for transmission.
    _modulate.rgb = alpha * (vec3(1.f) - transmit);
#endif

    //Modulate the net coverage for composition by the transmission. This does not affect the color channels of the
    //transparent surface because the caller's BSDF model should have already taken into account if transmission modulates
    //reflection. See

    //McGuire and Enderton, Colored Stochastic Shadow Maps, ACM I3D, February 2011
    //http://graphics.cs.williams.edu/papers/CSSM/

    //for a full explanation and derivation.
    //premultipliedReflect.a *= 1.0f - saturate((transmit.r + transmit.g + transmit.b) * 0.33f);

    // You may need to adjust the w function if you have a very large or very small view volume; see the paper and
    // presentation slides at http://jcgt.org/published/0002/02/09/ 
    // Intermediate terms to be cubed
    const float a = min(1.f, premultipliedReflect.a) * 8.f + 0.01f;
    float b = -gl_FragCoord.z * 0.95f + 1.f;

    // If your scene has a lot of content very close to the far plane, then include this line (one rsqrt instruction):
    //b /= sqrt(1e4 * abs(viewSpaceZ));

    const float w = clamp(a * a * a * 1e3 * b * b * b, 1e-2, 3e2);

    _accum = premultipliedReflect * w;
    _revealage = premultipliedReflect.a;
}
#endif //OIT_PASS

void writeAdditionalData(in vec2 packedNormalWV, in vec3 specularColour, in vec3 MetalnessRoughnessProbeID) {
#if defined(MAIN_DISPLAY_PASS)
    _specularDataOut = specularColour;
    _matDataOut.rg = packedNormalWV;
    _matDataOut.b = packVec2(MetalnessRoughnessProbeID.xy);
#if defined(NO_IBL)
    _matDataOut.a = float(PROBE_ID_NO_REFLECTIONS);
#elif defined(NO_ENV_MAPPING)
    _matDataOut.a = float(PROBE_ID_NO_ENV_REFLECTIONS);
#elif defined(NO_SSR)
    _matDataOut.a = float(PROBE_ID_NO_SSR);
#else // NO_IBL
    _matDataOut.a = MetalnessRoughnessProbeID.z;
#endif // NO_IBL
#if defined(NO_SSAO)
    _matDataOut.a = -_matDataOut.a;
#endif //NO_SSAO
#endif //MAIN_DISPLAY_PASS
}

void writeScreenColour(in vec4 colour, in vec3 normalWV, in vec3 specularColour, in vec3 MetalnessRoughnessProbeID) {
#if defined(OIT_PASS)
    const vec3 transmit = vec3(0.f);//texture(texTransmitance, dvd_screenPositionNormalised).rgb;
    const float viewSpaceZ = 1.f;// ViewSpaceZ(texture(texDepthMap, dvd_screenPositionNormalised).r, dvd_InverseProjectionMatrix);
    writePixel(vec4(colour.rgb * colour.a, colour.a), transmit, viewSpaceZ);
    if (colour.a < Z_TEST_SIGMA) {
        writeAdditionalData(vec2(MIN_FLOAT_OUTPUT), vec3(MIN_FLOAT_OUTPUT), vec3(MIN_FLOAT_OUTPUT));
        return;
    }
#else //OIT_PASS
    _colourOut = colour;
#endif //OIT_PASS
    writeAdditionalData(packNormal(normalWV), specularColour, MetalnessRoughnessProbeID);
}

void writeScreenColour(in vec4 colour, in vec3 normalWV, in vec3 specularColour) {
    writeScreenColour(colour, normalWV, specularColour, vec3(0.f, 1.f, 0.f));
}

void writeScreenColour(in vec4 colour, in vec3 normalWV) {
    writeScreenColour(colour, normalWV, vec3(0.f), vec3(0.f, 1.f, 0.f));
}
#endif //_OUTPUT_FRAG_
