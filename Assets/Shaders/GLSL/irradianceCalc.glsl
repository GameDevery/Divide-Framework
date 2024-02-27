--Compute.Irradiance

#include "pbrHelpers.comp"

#define THREADS 8

DESCRIPTOR_SET_RESOURCE(PER_DRAW, 0) uniform samplerCubeArray s_source;
DESCRIPTOR_SET_RESOURCE_LAYOUT(PER_DRAW, 12, rgba16f) uniform ACCESS_W imageCubeArray s_target;

layout(local_size_x = THREADS, local_size_y = THREADS, local_size_z = 1) in;
void main()
{
    for (uint i = 0; i < 6; ++i) 
    {
        ivec3 globalId = ivec3(gl_GlobalInvocationID.xy, i);

        vec3 N = normalize(toWorldCoords(globalId, imgSizeX));

        vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
        const vec3 right = normalize(cross(up, N));
        up = cross(N, right);

        vec3 color = vec3(0.f);
        uint sampleCount = 0u;
        float deltaPhi = TWO_M_PI / 360.0;
        float deltaTheta = M_PI_DIV_2 / 90.0;
        for (float phi = 0.0; phi < TWO_M_PI; phi += deltaPhi) {
            for (float theta = 0.0; theta < M_PI_DIV_2; theta += deltaTheta) {
                // Spherical to World Space in two steps...
                vec3 tempVec = cos(phi) * right + sin(phi) * up;
                vec3 sampleVector = cos(theta) * N + sin(theta) * tempVec;
                color += textureLod(s_source, vec4(sampleVector, layerIndex), 0).rgb * cos(theta) * sin(theta);
                sampleCount++;
            }
        }

        imageStore(s_target, ivec3(gl_GlobalInvocationID.xy, (layerIndex * 6) + i), vec4( M_PI * color / float(sampleCount), 1.f));
    }
}

--Compute.LUT

#include "pbrHelpers.comp"

#define THREADS 16

DESCRIPTOR_SET_RESOURCE_LAYOUT(PER_DRAW, 12, rg16f) uniform ACCESS_W image2D s_target;

// Karis 2014
vec2 integrateBRDF(float roughness, float NoV)
{
    vec3 V;
    V.x = sqrt(1.0 - NoV * NoV); // sin
    V.y = 0.0;
    V.z = NoV; // cos

    // N points straight upwards for this integration
    const vec3 N = vec3(0.0, 0.0, 1.0);

    float A = 0.0;
    float B = 0.0;
    const int numSamples = 1024;

    for (int i = 0; i < numSamples; i++) {
        vec2 Xi = hammersley(i, numSamples);
        // Sample microfacet direction
        vec3 H = importanceSampleGGX(Xi, roughness, N);

        // Get the light direction
        vec3 L = 2.0 * dot(V, H) * H - V;

        float NoL = Saturate(dot(N, L));
        float NoH = Saturate(dot(N, H));
        float VoH = Saturate(dot(V, H));

        if (NoL > 0.0) {
            // Terms besides V are from the GGX PDF we're dividing by
            float V_pdf = V_SmithGGXCorrelated(NoV, NoL, roughness) * VoH * NoL / NoH;
            float Fc = pow(1.0 - VoH, 5.0);
            A += (1.0 - Fc) * V_pdf;
            B += Fc * V_pdf;
        }
    }

    return 4.0 * vec2(A, B) / float(numSamples);
}


layout(local_size_x = THREADS, local_size_y = THREADS, local_size_z = 1) in;
void main()
{
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = vec2(gl_GlobalInvocationID.xy + 1) / vec2(imageSize(s_target).xy);
    float mu = uv.x;
    float a = uv.y;


    // Output to screen
    vec2 res = integrateBRDF(a, mu);

    // Scale and Bias for F0 (as per Karis 2014)
    imageStore(s_target, ivec2(gl_GlobalInvocationID.xy), vec4(res, 0.0, 0.0));
}

--Compute.PreFilter

#include "pbrHelpers.comp"

#define THREADS 8
#define NUM_SAMPLES 64u

DESCRIPTOR_SET_RESOURCE(PER_DRAW, 0) uniform samplerCubeArray s_source;
DESCRIPTOR_SET_RESOURCE_LAYOUT(PER_DRAW, 12, rgba16f) uniform ACCESS_W imageCubeArray s_target;

// From Karis, 2014
vec3 prefilterEnvMap(in vec3 R)
{
    // Isotropic approximation: we lose stretchy reflections :(
    vec3 N = R;
    vec3 V = R;

    vec3 prefilteredColor = vec3(0.0);
    float totalWeight = 0.f;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        vec2 Xi = hammersley(i, NUM_SAMPLES);
        vec3 H = importanceSampleGGX(Xi, roughness, N);
        float VoH = dot(V, H);
        float NoH = VoH; // Since N = V in our approximation
        // Use microfacet normal H to find L
        vec3 L = 2.0 * VoH * H - V;
        float NoL = Saturate(dot(N, L));
        // Clamp 0 <= NoH <= 1
        NoH = Saturate(NoH);

        if (NoL > 0.0) {
            // Based off https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch20.html
            // Typically you'd have the following:
            // float pdf = D_GGX(NoH, roughness) * NoH / (4.0 * VoH);
            // but since V = N => VoH == NoH
            float pdf = D_GGX(NoH, roughness) / 4.0 + 0.001;
            // Solid angle of current sample -- bigger for less likely samples
            float omegaS = 1.0 / (float(NUM_SAMPLES) * pdf);
            // Solid angle of texel
            float omegaP = 4.0 * M_PI / (6.0 * imgSizeX * imgSizeX);
            // Mip level is determined by the ratio of our sample's solid angle to a texel's solid angle
            float mipLevelTemp = max(0.5 * log2(omegaS / omegaP), 0.0);
            prefilteredColor += textureLod(s_source, vec4(L, layerIndex), mipLevelTemp).rgb * NoL;
            totalWeight += NoL;
        }
    }
    return prefilteredColor / totalWeight;
}

layout(local_size_x = THREADS, local_size_y = THREADS, local_size_z = 1) in;
void main()
{
    const float mipImageSize = imgSizeX / pow(2.f, mipLevel);
    if (gl_GlobalInvocationID.x >= mipImageSize || gl_GlobalInvocationID.y >= mipImageSize) {
        return;
    }

    for (uint i = 0; i < 6; ++i) {
        vec3 R = normalize(toWorldCoords(ivec3(gl_GlobalInvocationID.xy, i), mipImageSize));

        // Don't need to integrate for roughness == 0, since it's a perfect reflector
        vec4 color;
        if (roughness == 0.f)
        {
            color = textureLod(s_source, vec4(R, layerIndex), 0);
        }
        else
        {
            color = vec4(prefilterEnvMap(R), 1.f);
        }

        // We access our target cubemap as a 2D texture array, where z is the face index
        imageStore(s_target, ivec3(gl_GlobalInvocationID.xy, (layerIndex * 6) + i), color);
    }
}

