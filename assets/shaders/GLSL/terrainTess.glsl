--Vertex

#define NEED_MATERIAL_DATA

#include "nodeBufferedInput.cmn"
#include "terrainUtils.cmn"

layout(location = ATTRIB_POSITION) in vec4 inVertexData;
layout(location = ATTRIB_COLOR)    in vec4 inColourData;

layout(location = 10) out vec4 vtx_adjancency;
layout(location = 11) out float vtx_tileSize;
layout(location = 12) out flat uint vtx_ringID;

void main(void)
{
    // Calculate texture coordinates (u,v) relative to entire terrain
    const float iv = floor(gl_VertexID * INV_CONTROL_VTX_PER_TILE_EDGE);
    const float iu = gl_VertexID - iv * CONTROL_VTX_PER_TILE_EDGE;
    const float u = iu / (CONTROL_VTX_PER_TILE_EDGE - 1.0f);
    const float v = iv / (CONTROL_VTX_PER_TILE_EDGE - 1.0f);

    vtx_tileSize = inVertexData.z;
    vtx_adjancency = inColourData;
    vtx_ringID = uint(inVertexData.w);

    const vec2 posXZ = vec2(u, v) * vtx_tileSize + inVertexData.xy;

    gl_Position = vec4(posXZ.x, GetHeight(WorldXZtoHeightUV(posXZ)), posXZ.y, 1.f);
}

--TessellationC

#define NEED_MATERIAL_DATA
#include "nodeBufferedInput.cmn"
#include "terrainUtils.cmn"

// Most of the stuff here is from nVidia's DX11 terrain tessellation sample
layout(location = 10) in vec4 vtx_adjancency[];
layout(location = 11) in float vtx_tileSize[];
layout(location = 12) in flat uint vtx_ringID[];

// Outputs
layout(vertices = 4) out;
layout(location = 10) out float tcs_tileSize[];
layout(location = 11) out flat uint tcs_ringID[];

#if defined(TOGGLE_DEBUG) || defined(TOGGLE_TESS_LEVEL)
layout(location = 12) out vec3[5] tcs_debugColour[];
#if defined(TOGGLE_DEBUG)
#if defined(TOGGLE_NORMALS)
#undef MAX_TESS_LEVEL
#define MAX_TESS_LEVEL 1
#endif //TOGGLE_NORMALS
#endif //TOGGLE_DEBUG
#endif //TOGGLE_DEBUG || TOGGLE_TESS_LEVEL

#if !defined(MAX_TESS_LEVEL)
#define MAX_TESS_LEVEL 64
#endif //MAX_TESS_LEVEL

float SphereToScreenSpaceTessellation(in vec3 w0, in vec3 w1, in float diameter, in mat4 worldViewMat)
{
    const vec3 centre = 0.5f * (w0 + w1);
    const vec4 view0 = worldViewMat * vec4(centre, 1.f);
    const vec4 view1 = view0 + vec4(WORLD_SCALE_X * diameter, 0.f, 0.f, 0.f);

    vec4 clip0 = dvd_ProjectionMatrix * view0; // to clip space
    vec4 clip1 = dvd_ProjectionMatrix * view1; // to clip space
    clip0 /= clip0.w;                          // project
    clip1 /= clip1.w;                          // project
    //clip0.xy = clip0.xy * 0.5f + 0.5f;       // to NDC (DX11 sample skipped this)
    //clip1.xy = clip1.xy * 0.5f + 0.5f;       // to NDC (DX11 sample skipped this)
    clip0.xy *= dvd_screenDimensions;          // to pixels
    clip1.xy *= dvd_screenDimensions;          // to pixels

    return clamp(distance(clip0, clip1) / dvd_tessTriangleWidth, 1.f, float(MAX_TESS_LEVEL));
}


// The adjacency calculations ensure that neighbours have tessellations that agree.
// However, only power of two sizes *seem* to get correctly tessellated with no cracks.
float SmallerNeighbourAdjacencyClamp(in float tess) {
    // Clamp to the nearest larger power of two.  Any power of two works; larger means that we don't lose detail.
    // Output is [4, MAX_TESS_LEVEL]. Our smaller neighbour's min tessellation is pow(2,1) = 2.  As we are twice its size, we can't go below 4.
    return max(4, pow(2, ceil(log2(tess))));
}

float LargerNeighbourAdjacencyClamp(in float tess) {
    // Clamp to the nearest larger power of two.  Any power of two works; larger means that we don't lose detail.
    // Our larger neighbour's max tessellation is MAX_TESS_LEVEL; as we are half its size, our tessellation must max out
    // at MAX_TESS_LEVEL / 2, otherwise we could be over-tessellated relative to the neighbour.  Output is [2,MAX_TESS_LEVEL].
    return clamp(pow(2, ceil(log2(tess))), 2.f, MAX_TESS_LEVEL * 0.5f);
}

float SmallerNeighbourAdjacencyFix(in int idx0, in int idx1, in float diameter, in mat4 worldViewMat) {
    vec3 p0 = gl_in[idx0].gl_Position.xyz;
    vec3 p1 = gl_in[idx1].gl_Position.xyz;
    p0.y = p1.y = GetHeight(WorldXZtoHeightUV(p0.xz));

    return SmallerNeighbourAdjacencyClamp(SphereToScreenSpaceTessellation(p0, p1, diameter, worldViewMat));
}

float LargerNeighbourAdjacencyFix(in int idx0, in int idx1, in int patchIdx, in float diameter, in mat4 worldViewMat) {
    vec3 p0 = gl_in[idx0].gl_Position.xyz;
    vec3 p1 = gl_in[idx1].gl_Position.xyz;

    // We move one of the corner vertices in 2D (x,z) to match where the corner vertex is 
    // on our larger neighbour.  We move p0 or p1 depending on the even/odd patch index.
    //
    // Larger neighbour
    // +-------------------+
    // +---------+
    // p0   Us   p1 ---->  +		Move p1
    // |    0    |    1    |		patchIdx % 2 
    //
    //           +---------+
    // +  <----  p0   Us   p1		Move p0
    // |    0    |    1    |		patchIdx % 2 
    //
    if (patchIdx % 2 != 0) {
        p0 += (p0 - p1);
    } else { 
        p1 += (p1 - p0);
    }

    // Having moved the vertex in (x,z), its height is no longer correct.
    p0.y = p1.y = GetHeight(WorldXZtoHeightUV(p0.xz));
    // Half the tessellation because the edge is twice as long.
    return LargerNeighbourAdjacencyClamp(0.5f * SphereToScreenSpaceTessellation(p0, p1, 2 * diameter, worldViewMat));
}

bool SphereInFrustum(in vec3 pos, in float r, in mat4 worldMat) {
    const float minCmp = -0.15f;

    const vec4 posW = worldMat * vec4(pos, 1.f);
    for (int i = 0; i < 6; i++) {
        if (dot(posW, dvd_frustumPlanes[i]) + r < minCmp) {
            return false;
        }
    }
    return true;
}


void main(void)
{
    const vec3 centre = 0.25f * (gl_in[0].gl_Position.xyz +
                                 gl_in[1].gl_Position.xyz +
                                 gl_in[2].gl_Position.xyz +
                                 gl_in[3].gl_Position.xyz);

    const mat4 worldMat = dvd_WorldMatrix * dvd_terrainWorld;

    const vec4 pos0 = worldMat * gl_in[0].gl_Position;
    const vec4 pos1 = worldMat * gl_in[2].gl_Position;
    const float radius = length(pos0.xyz - pos1.xyz) * 0.5f;

    if (!SphereInFrustum(centre, radius, worldMat))
    {
          gl_TessLevelInner[0] = gl_TessLevelInner[1] = -1;
          gl_TessLevelOuter[0] = gl_TessLevelOuter[1] = -1;
          gl_TessLevelOuter[2] = gl_TessLevelOuter[3] = -1;
    }
    else
    {
#if MAX_TESS_LEVEL > 1
        const mat4 worldViewMat = dvd_ViewMatrix * worldMat;
        const float sideLen = max(abs(gl_in[1].gl_Position.x - gl_in[0].gl_Position.x), abs(gl_in[1].gl_Position.x - gl_in[2].gl_Position.x));
        // Outer tessellation level
        gl_TessLevelOuter[0] = SphereToScreenSpaceTessellation(gl_in[0].gl_Position.xyz, gl_in[1].gl_Position.xyz, sideLen, worldViewMat);
        gl_TessLevelOuter[1] = SphereToScreenSpaceTessellation(gl_in[3].gl_Position.xyz, gl_in[0].gl_Position.xyz, sideLen, worldViewMat);
        gl_TessLevelOuter[2] = SphereToScreenSpaceTessellation(gl_in[2].gl_Position.xyz, gl_in[3].gl_Position.xyz, sideLen, worldViewMat);
        gl_TessLevelOuter[3] = SphereToScreenSpaceTessellation(gl_in[1].gl_Position.xyz, gl_in[2].gl_Position.xyz, sideLen, worldViewMat);

#if !defined(LOW_QUALITY)
        // Edges that need adjacency adjustment are identified by the per-instance ip[0].adjacency 
        // scalars, in *conjunction* with a patch ID that puts them on the edge of a tile.
        const int PatchID = gl_PrimitiveID;
        ivec2 patchXY;
        patchXY.y = PatchID / PATCHES_PER_TILE_EDGE;
        patchXY.x = PatchID - patchXY.y * PATCHES_PER_TILE_EDGE;

        // Identify patch edges that are adjacent to a patch of a different size.  The size difference
        // is encoded in _in[n].adjacency, either 0.5, 1.0 or 2.0.
        // neighbourMinusX refers to our adjacent neighbour in the direction of -ve x.  The value
        // is the neighbour's size relative to ours.  Similarly for plus and Y, etc.  You really
        // need a diagram to make sense of the adjacency conditions in the if statements. :-(
        if (patchXY.x == 0) {
            if (vtx_adjancency[0].x < 0.55f) {
                // Deal with neighbours that are smaller.
                gl_TessLevelOuter[0] = SmallerNeighbourAdjacencyFix(0, 1, sideLen, worldViewMat);
            } else if (vtx_adjancency[0].x > 1.1f) {
                // Deal with neighbours that are larger than us.
                gl_TessLevelOuter[0] = LargerNeighbourAdjacencyFix(0, 1, patchXY.y, sideLen, worldViewMat);
            }
        } else if (patchXY.x == PATCHES_PER_TILE_EDGE - 1) {
            if (vtx_adjancency[0].z < 0.55f) {
                gl_TessLevelOuter[2] = SmallerNeighbourAdjacencyFix(2, 3, sideLen, worldViewMat);
            } else if (vtx_adjancency[0].z > 1.1f) {
                gl_TessLevelOuter[2] = LargerNeighbourAdjacencyFix(3, 2, patchXY.y, sideLen, worldViewMat);
            }
        }

        if (patchXY.y == 0) {
            if (vtx_adjancency[0].y < 0.55f) {
                gl_TessLevelOuter[1] = SmallerNeighbourAdjacencyFix(3, 0, sideLen, worldViewMat);
            } else if (vtx_adjancency[0].y > 1.1f) {
                gl_TessLevelOuter[1] = LargerNeighbourAdjacencyFix(0, 3, patchXY.x, sideLen, worldViewMat);	// NB: irregular index pattern - it's correct.
            }
        } else if (patchXY.y == PATCHES_PER_TILE_EDGE - 1) {
            if (vtx_adjancency[0].w < 0.55f) {
                gl_TessLevelOuter[3] = SmallerNeighbourAdjacencyFix(1, 2, sideLen, worldViewMat);
            } else if (vtx_adjancency[0].w > 1.1f) {
                gl_TessLevelOuter[3] = LargerNeighbourAdjacencyFix(1, 2, patchXY.x, sideLen, worldViewMat);	// NB: irregular index pattern - it's correct.
            }
        }
#endif //LOW_QUALITY

#else //MAX_TESS_LEVEL > 1
        // Outer tessellation level
        gl_TessLevelOuter[0] = gl_TessLevelOuter[1] = gl_TessLevelOuter[2] = gl_TessLevelOuter[3] = 1.0f;
#endif //MAX_TESS_LEVEL > 1

        // Inner tessellation level
        gl_TessLevelInner[0] = 0.5f * (gl_TessLevelOuter[0] + gl_TessLevelOuter[3]);
        gl_TessLevelInner[1] = 0.5f * (gl_TessLevelOuter[2] + gl_TessLevelOuter[1]);

        // Pass the patch verts along
        gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
        // The VS displaces y for LOD calculations.  We drop it here so as not to displace twice in the DS.
        gl_out[gl_InvocationID].gl_Position.y = 0.0f;

        tcs_tileSize[gl_InvocationID] = vtx_tileSize[gl_InvocationID];
        tcs_ringID[gl_InvocationID] = vtx_ringID[gl_InvocationID];
#if defined(TOGGLE_DEBUG) || defined(TOGGLE_TESS_LEVEL)
        // Output tessellation level (used for wireframe coloring)
        // These are one colour for each tessellation level and linear graduations between.
        const vec3 DEBUG_COLOURS[6] =
        {
            vec3(0,0,1), //  2 - blue   
            vec3(0,1,1), //  4 - cyan   
            vec3(0,1,0), //  8 - green  
            vec3(1,1,0), // 16 - yellow 
            vec3(1,0,1), // 32 - purple
            vec3(1,0,0), // 64 - red
        };

        tcs_debugColour[gl_InvocationID][0] = DEBUG_COLOURS[clamp(int(log2(gl_TessLevelOuter[0])), 0, 5)];
        tcs_debugColour[gl_InvocationID][1] = DEBUG_COLOURS[clamp(int(log2(gl_TessLevelOuter[1])), 0, 5)];
        tcs_debugColour[gl_InvocationID][2] = DEBUG_COLOURS[clamp(int(log2(gl_TessLevelOuter[2])), 0, 5)];
        tcs_debugColour[gl_InvocationID][3] = DEBUG_COLOURS[clamp(int(log2(gl_TessLevelOuter[3])), 0, 5)];
        tcs_debugColour[gl_InvocationID][4] = DEBUG_COLOURS[clamp(int(log2(gl_TessLevelInner[0])), 0, 5)];
#endif //TOGGLE_DEBUG
    }
}

--TessellationE

layout(quads, fractional_even_spacing, cw) in;

#define NEED_MATERIAL_DATA
#include "nodeBufferedInput.cmn"
#include "terrainUtils.cmn"

layout(location = 10) in float tcs_tileSize[];
layout(location = 11) in flat uint tcs_ringID[];

#if defined(TOGGLE_DEBUG) || defined(TOGGLE_TESS_LEVEL)
layout(location = 12) in vec3[5] tcs_debugColour[];
layout(location = 12) out vec3  tes_debugColour;

#if defined(TOGGLE_DEBUG)
layout(location = 10) out float tes_tileSize;
layout(location = 11) out flat uint tes_ringID;
layout(location = 13) out float tes_PatternValue;
#endif //TOGGLE_DEBUG

#endif //TOGGLE_DEBUG || TOGGLE_TESS_LEVEL

#if !defined(TOGGLE_DEBUG)
layout(location = 10) out flat uint dvd_LoD;
#endif //!TOGGLE_DEBUG

vec3 Bilerp(vec3 v0, vec3 v1, vec3 v2, vec3 v3, vec2 i)
{
    const vec3 bottom = lerp(v0, v3, i.x);
    const vec3 top = lerp(v1, v2, i.x);
    return lerp(bottom, top, i.y);
}

#if defined(TOGGLE_DEBUG) || defined(TOGGLE_TESS_LEVEL)
vec3 BilerpColour(vec3 c0, vec3 c1, vec3 c2, vec3 c3, vec2 UV) {
    const vec3 left = lerp(c0, c1, UV.y);
    const vec3 right = lerp(c2, c3, UV.y);
    return lerp(left, right, UV.x);
}

vec3 LerpDebugColours(in vec3 cIn[5], vec2 uv) {
    if (uv.x < 0.5f && uv.y < 0.5f) {
        return BilerpColour(0.5f * (cIn[0] + cIn[1]), cIn[0], cIn[1], cIn[4], 2 * uv);
    } else if (uv.x < 0.5f && uv.y >= 0.5f) {
        return BilerpColour(cIn[0], 0.5f * (cIn[0] + cIn[3]), cIn[4], cIn[3], 2 * (uv - vec2(0.f, 0.5f)));
    } else if (uv.x >= 0.5f && uv.y < 0.5f) {
        return BilerpColour(cIn[1], cIn[4], 0.5f * (cIn[2] + cIn[1]), cIn[2], 2 * (uv - vec2(0.5f, 0.f)));
    } else {// x >= 0.5f && y >= 0.5f
        return BilerpColour(cIn[4], cIn[3], cIn[2], 0.5f * (cIn[2] + cIn[3]), 2 * (uv - vec2(0.5f, 0.5f)));
    }
}
#endif //TOGGLE_DEBUG || TOGGLE_TESS_LEVEL

void main()
{
    // Calculate the vertex position using the four original points and interpolate depending on the tessellation coordinates.
    const vec3 pos = Bilerp(gl_in[0].gl_Position.xyz,
                            gl_in[1].gl_Position.xyz,
                            gl_in[2].gl_Position.xyz,
                            gl_in[3].gl_Position.xyz,
                            gl_TessCoord.xy);
    _out._texCoord = WorldXZtoHeightUV(pos.xz);
    _out._vertexW = dvd_WorldMatrix * dvd_terrainWorld * vec4(pos.x, GetHeight(_out._texCoord), pos.z, 1.0f);
    _out._vertexWV = dvd_ViewMatrix * _out._vertexW;
    setClipPlanes(); //Only need world vertex position for clipping
    _out._normalWV = dvd_NormalMatrixW(dvd_Transforms[TRANSFORM_IDX]) * getNormal(_out._texCoord);

#if !defined(PRE_PASS) && !defined(SHADOW_PASS)
    _out._viewDirectionWV = mat3(dvd_ViewMatrix) * normalize(dvd_cameraPosition.xyz - _out._vertexW.xyz);
#endif //PRE_PASS && SHADOW_PASS

#if defined(TOGGLE_DEBUG) || defined(TOGGLE_TESS_LEVEL)
    tes_debugColour = LerpDebugColours(tcs_debugColour[0], gl_TessCoord.xy);
#endif //TOGGLE_DEBUG || TOGGLE_TESS_LEVEL

#if defined(TOGGLE_DEBUG)
    const int PatchID = gl_PrimitiveID;
    ivec2 patchXY;
    patchXY.y = PatchID / PATCHES_PER_TILE_EDGE;
    patchXY.x = PatchID - patchXY.y * PATCHES_PER_TILE_EDGE;

    tes_tileSize = tcs_tileSize[0];
    tes_ringID = tcs_ringID[0];
    tes_PatternValue = 0.5f * ((patchXY.x + patchXY.y) % 2);

    gl_Position = _out._vertexW;
#else //TOGGLE_DEBUG
    dvd_LoD = tcs_ringID[0];
    gl_Position = dvd_ProjectionMatrix * _out._vertexWV;
#endif //TOGGLE_DEBUG
}

--Geometry

#define NEED_MATERIAL_DATA
#include "nodeBufferedInput.cmn"

layout(triangles) in;

layout(location = 10) in float tes_tileSize[];
layout(location = 11) in flat uint tes_ringID[];
layout(location = 12) in vec3 tes_debugColour[];
layout(location = 13) in float tes_PatternValue[];

layout(location = 10) out flat uint dvd_LoD;
layout(location = 11) out vec3 gs_wireColor;
layout(location = 12) noperspective out vec4 gs_edgeDist;  //w - patternValue

#if defined(TOGGLE_NORMALS)
layout(line_strip, max_vertices = 18) out;
#else //TOGGLE_NORMALS
layout(triangle_strip, max_vertices = 4) out;
#endif //TOGGLE_NORMALS

vec4 getWVPPositon(in int i) {
    return dvd_ViewProjectionMatrix * gl_in[i].gl_Position;
}

void PerVertex(in int i, in vec3 edge_dist) {
    PassData(i);
    gl_Position = getWVPPositon(i);

    dvd_LoD = tes_ringID[i];
    
    gs_edgeDist = vec4(i == 0 ? edge_dist.x : 0.0,
                       i == 1 ? edge_dist.y : 0.0,
                       i >= 2 ? edge_dist.z : 0.0,
                       tes_PatternValue[i]);
    setClipPlanes();
}

void main(void)
{
    // Calculate edge distances for wireframe
    vec3 edge_dist = vec3(0.0);
    {
        vec4 pos0 = getWVPPositon(0);
        vec4 pos1 = getWVPPositon(1);
        vec4 pos2 = getWVPPositon(2);

        vec2 p0 = vec2(dvd_ViewPort.zw * (pos0.xy / pos0.w));
        vec2 p1 = vec2(dvd_ViewPort.zw * (pos1.xy / pos1.w));
        vec2 p2 = vec2(dvd_ViewPort.zw * (pos2.xy / pos2.w));

        float a = length(p1 - p2);
        float b = length(p2 - p0);
        float c = length(p1 - p0);
        float alpha = acos((b * b + c * c - a * a) / (2.0 * b * c));
        float beta = acos((a * a + c * c - b * b) / (2.0 * a * c));
        edge_dist.x = abs(c * sin(beta));
        edge_dist.y = abs(c * sin(alpha));
        edge_dist.z = abs(b * sin(alpha));
    }

    const int count = gl_in.length();

#if defined(TOGGLE_NORMALS)
    const float sizeFactor = 0.75f;
    for (int i = 0; i < count; ++i) {
        // In world space
        const vec3 N = getNormal(_in[i]._texCoord);
        const vec3 B = cross(vec3(0.0f, 0.0f, 1.0f), N);
        const vec3 T = cross(N, B);

        vec3 P = gl_in[i].gl_Position.xyz;
        PerVertex(i, edge_dist);

        { // normals
            gs_wireColor = vec3(0.0f, 0.0f, 1.0f);
            gl_Position = dvd_ViewProjectionMatrix * vec4(P, 1.0);
            EmitVertex();

            PerVertex(1, edge_dist);
            gs_wireColor = vec3(0.0f, 0.0f, 1.0f);
            gl_Position = dvd_ViewProjectionMatrix * vec4(P + N * sizeFactor, 1.0);
            EmitVertex();

            EndPrimitive();
        }
        { // binormals
            gs_wireColor = vec3(0.0f, 1.0f, 0.0f);
            gl_Position = dvd_ViewProjectionMatrix * vec4(P, 1.0);
            EmitVertex();

            gs_wireColor = vec3(0.0f, 1.0f, 0.0f);
            gl_Position = dvd_ViewProjectionMatrix * vec4(P + B * sizeFactor, 1.0);
            EmitVertex();

            EndPrimitive();
        }
        { // tangents
            gs_wireColor = vec3(1.0f, 0.0f, 0.0f);
            gl_Position = dvd_ViewProjectionMatrix * vec4(P, 1.0);
            EmitVertex();

            gs_wireColor = vec3(1.0f, 0.0f, 0.0f);
            gl_Position = dvd_ViewProjectionMatrix * vec4(P + T * sizeFactor, 1.0);
            EmitVertex();

            EndPrimitive();
        }
    }
#else //TOGGLE_NORMALS

    // Output verts
    for (int i = 0; i < count; ++i) {
        PerVertex(i, edge_dist);
        gs_wireColor = tes_debugColour[i];
        EmitVertex();
    }

    // This closes the triangle
    PerVertex(0, edge_dist);
    gs_wireColor = tes_debugColour[0];
    EmitVertex();

    EndPrimitive();

#endif //TOGGLE_NORMALS
}

--Fragment.PrePass

layout(location = 10) in flat uint dvd_LoD;

#if defined(TOGGLE_DEBUG)
layout(location = 11) in vec3 gs_wireColor;
layout(location = 12) noperspective in vec4 gs_edgeDist;  //w - patternValue
#endif //TOGGLE_DEBUG

#define USE_CUSTOM_NORMAL_MAP
#define USE_CUSTOM_TBN

#include "prePass.frag"
#include "terrainSplatting.frag"

void main(void)
{
    vec3 normalWV;
    vec4 albedo = BuildTerrainData(normalWV);

    writeGBuffer(1.0f, VAR._texCoord, normalWV);
}

--Fragment

layout(early_fragment_tests) in;

#define USE_CUSTOM_ROUGHNESS
#define SHADOW_INTENSITY_FACTOR 0.5f
#define SSAO_LOD_0
#define USE_CUSTOM_TBN

layout(location = 10) in flat uint dvd_LoD;

#if defined(TOGGLE_DEBUG)

layout(location = 11) in vec3 gs_wireColor;
layout(location = 12) noperspective in vec4 gs_edgeDist;  //w - patternValue

#else //TOGGLE_DEBUG

#if defined(TOGGLE_TESS_LEVEL)
layout(location = 12) in vec3 tes_debugColour;
#endif //TOGGLE_TESS_LEVEL

#endif //TOGGLE_DEBUG

#include "BRDF.frag"
#include "terrainSplatting.frag"
#include "output.frag"
#include "terrainUtils.cmn"

float _private_roughness = 0.0f;
vec3 getOcclusionMetallicRoughness(in NodeMaterialData data, in vec2 uv) {
    return vec3(0.0f, 0.0f, _private_roughness);
}

void main(void) {

    vec3 normalWV;
    const vec4 albedo = BuildTerrainData(normalWV);
    _private_roughness = albedo.a;

#if defined (TOGGLE_LODS)
    vec4 colourOut = vec4(0.0f);
    switch (dvd_LoD) {
        case 0  : colourOut = vec4(1.0f, 0.0f, 0.0f, 1.0f); break;
        case 1  : colourOut = vec4(0.0f, 1.0f, 0.0f, 1.0f); break;
        case 2  : colourOut = vec4(0.0f, 0.0f, 1.0f, 1.0f); break;
        case 3  : colourOut = vec4(0.0f, 1.0f, 1.0f, 1.0f); break;
        case 4  : colourOut = vec4(1.0f, 0.0f, 1.0f, 1.0f); break;
        default : colourOut = vec4(turboColormap(float(dvd_LoD)), 1.0f); break;
    }
#else  //TOGGLE_LODS
#if defined(TOGGLE_TESS_LEVEL)
    vec4 colourOut = vec4(tes_debugColour, 1.0f);
#else //TOGGLE_TESS_LEVEL
#if defined(TOGGLE_BLEND_MAP)
    vec4 colourOut = vec4(vec3(0.f), 1.0f);
    uint offset = 0;
    for (uint i = 0; i < MAX_TEXTURE_LAYERS; ++i) {
        colourOut.rgb += texture(texOpacityMap, vec3(VAR._texCoord, i)).rgb;
    }
#else //TOGGLE_BLEND_MAP
    NodeMaterialData data = dvd_Materials[MATERIAL_IDX];
    vec4 colourOut = getPixelColour(vec4(albedo.rgb, 1.0f), data, normalWV, VAR._texCoord, dvd_LoD);
#endif //TOGGLE_BLEND_MAP
#endif //TOGGLE_TESS_LEVEL

#if defined (TOGGLE_DEBUG)

#if defined(TOGGLE_NORMALS)
    colourOut = vec4(gs_wireColor, 1.0f);
#else //TOGGLE_NORMALS
    float val = 0.5f * gs_edgeDist.w;
    colourOut.rgb *= val;// vec4(vec3(val), 1.0f);

    const float LineWidth = 0.75f;
    const float d = min(min(gs_edgeDist.x, gs_edgeDist.y), gs_edgeDist.z);
    colourOut = mix(vec4(gs_wireColor, 1.0f), colourOut, smoothstep(LineWidth - 1, LineWidth + 1, d));
#endif //TOGGLE_NORMALS

#endif //TOGGLE_DEBUG

#endif //TOGGLE_LODS
    writeScreenColour(colourOut);
}