-- Vertex

#include "vbInputData.vert"
#include "lightingDefaults.vert"

out flat int _underwater;
out vec3 _pixToEye;
out vec4 _vertexWVP;

void main(void)
{
    computeData();
    
    computeLightVectors();

    _pixToEye  = -VAR._vertexWV.xyz;
    _vertexWVP = dvd_ProjectionMatrix * VAR._vertexWV;
    _underwater = dvd_cameraPosition.y < VAR._vertexW.y ? 1 : 0;

    gl_Position = _vertexWVP;
}

-- Fragment

in flat int _underwater;
in vec3 _pixToEye;
in vec4 _vertexWVP;

uniform vec2 _noiseTile;
uniform vec2 _noiseFactor;
uniform float _waterShininess;

#include "BRDF.frag"
#include "shadowMapping.frag"
#include "output.frag"

const float Eta = 0.15; //water

float Fresnel(in vec3 viewDir, in vec3 normal) {
    return _underwater == 1 ? 1.0 : 1.0 / pow(1.0 + dot(viewDir, normal), 5.0);
}

void main (void)
{  
#   define texWaterReflection texReflectPlanar
#   define texWaterRefraction texRefractPlanar
#   define texWaterNoiseNM texDiffuse0
#   define texWaterNoiseDUDV texDiffuse1

    const float kDistortion = 0.015;
    const float kRefraction = 0.09;

    vec4 uvReflection = _vertexWVP / _vertexWVP.w;
    uvReflection += vec4(1.0);
    uvReflection *= vec4(0.5);
    uvReflection = clamp(uvReflection, vec4(0.001), vec4(0.999));


    float time2 = float(dvd_time) * 0.00001;
    vec2 uvNormal0 = VAR._texCoord * _noiseTile;
    uvNormal0.s += time2;
    uvNormal0.t += time2;
    vec2 uvNormal1 = VAR._texCoord * _noiseTile;
    uvNormal1.s -= time2;
    uvNormal1.t += time2;

    vec3 normal0 = texture(texWaterNoiseNM, uvNormal0).rgb * 2.0 - 1.0;
    vec3 normal1 = texture(texWaterNoiseNM, uvNormal1).rgb * 2.0 - 1.0;
    vec3 normal = normalize(normal0 + normal1);

    vec2 uvFinalReflect = uvReflection.xy + _noiseFactor * normal.xy;
    vec2 uvFinalRefract = uvReflection.xy + _noiseFactor * normal.xy;


    /*vec4 distOffset = texture(texWaterNoiseDUDV, VAR._texCoord + vec2(time2)) * kDistortion;
    vec4 dudvColor = texture(texWaterNoiseDUDV, vec2(VAR._texCoord + distOffset.xy));
    dudvColor = normalize(dudvColor * 2.0 - 1.0) * kRefraction;

    vec3 normal = texture(texWaterNoiseNM, vec2(VAR._texCoord + distOffset.xy)).rgb;
    normal = normalize(normal * 2.0 - 1.0);*/

    vec4 mixFactor = vec4(clamp(Fresnel(normalize(_pixToEye), normalize(VAR._normalWV)), 0.0, 1.0));
    vec4 texColour = mix(texture(texWaterReflection, uvFinalReflect),
                         texture(texWaterRefraction, uvFinalRefract),
                         mixFactor);
    normal = normalize(getTBNMatrix() * normal);
#if 0
    mat4 colourMatrix = dvd_Matrices[VAR.dvd_baseInstance]._colourMatrix;
    writeOutput(getPixelColour(texColour, colourMatrix, normal), packNormal(normal));
#else
    writeOutput(texColour, packNormal(normal));
#endif


}

--Fragment.PrePass

void main() {

}