#version 450
#extension GL_ARB_separate_shader_objects : enable

//-----------------------------------------------------------------------------
// [SECTION] specialication constants
//-----------------------------------------------------------------------------

layout(constant_id = 0) const int iMeshVariantFlags = 0;
layout(constant_id = 1) const int iDataStride = 0;

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

// iMeshVariantFlags
const int PL_MESH_FORMAT_FLAG_HAS_POSITION   = 1 << 0;
const int PL_MESH_FORMAT_FLAG_HAS_NORMAL     = 1 << 1;

//-----------------------------------------------------------------------------
// [SECTION] bind group 0
//-----------------------------------------------------------------------------

layout(set = 0, binding = 0) uniform _plGlobalInfo
{
    vec4 tViewportSize;
    vec4 tCameraPos;
    mat4 tCameraView;
    mat4 tCameraProjection;
    mat4 tCameraViewProjection;
} tGlobalInfo;

layout(std140, set = 0, binding = 1) readonly buffer _tVertexBuffer
{
	vec4 atVertexData[];
} tVertexBuffer;

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    vec4   tColor;
    float  fThickness;
    int    iDataOffset;
    int    iVertexOffset;
    mat4   tModel;
} tObjectInfo;

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

// input
layout(location = 0) in vec3 inPos;


void
main() 
{
    const mat4 tMVP = tGlobalInfo.tCameraViewProjection * tObjectInfo.tModel;
    vec4 inPosition  = vec4(inPos, 1.0);
    vec3 inNormal    = vec3(0.0, 0.0, 0.0);

    const uint iVertexDataOffset = iDataStride * (gl_VertexIndex - tObjectInfo.iVertexOffset) + tObjectInfo.iDataOffset;
    int iCurrentAttribute = 0;
    if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_POSITION)) iCurrentAttribute++;
    if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_NORMAL)) { inNormal = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute].xyz;}

    vec4 tPos = tMVP * inPosition;
    vec4 tNorm = normalize(tMVP * vec4(inNormal, 0.0));
    tPos = vec4(tPos.xyz + tNorm.xyz * tObjectInfo.fThickness, tPos.w);
    gl_Position = tPos;
}