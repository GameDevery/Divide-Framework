#ifndef _PHONG_LIGHTING_FRAG_
#define _PHONG_LIGHTING_FRAG_

vec4 Phong(in vec4 lightColourAndAtt,
           in vec4 specular,
           in vec4 albedoAndShadow,
           in vec3 normalWV,
           in vec3 lightDirection)
{
    const float kPi = 3.14159265;
    const float kShininess = specular.a;

    const float NDotL = clamp(dot(normalize(lightDirection), normalWV), 0.0f, 1.0f);

    float specPower = 0.0;
#if !defined(NO_SPECULAR)
    if (NDotL > 0.0f) {
        const vec3 dvd_ViewDirNorm = normalize(-VAR._vertexWV.xyz);

#if defined(USE_SHADING_BLINN_PHONG)
        const float kEnergyConservation = (8.0 + kShininess) / (8.0 * kPi);

        const vec3 halfwayDir = normalize(lightDirection + dvd_ViewDirNorm);
        specPower = kEnergyConservation * pow(max(dot(normalWV, halfwayDir), 0.0), kShininess);
#else
        const float kEnergyConservation = (2.0 + kShininess) / (2.0 * kPi);

        const vec3 reflectDir = reflect(-lightDirection, normalWV);
        specPower = kEnergyConservation * pow(max(dot(dvd_ViewDirNorm, reflectDir), 0.0), kShininess);
#endif
    }
#endif

    vec3 lightColour = lightColourAndAtt.rgb * lightColourAndAtt.a;

    vec3 colorOut = (albedoAndShadow.rgb * saturate(NDotL) * lightColour) +
                    (specular.rgb * specPower * lightColour);

    return vec4(colorOut * albedoAndShadow.a, specPower);
}


#endif //_PHONG_LIGHTING_FRAG_