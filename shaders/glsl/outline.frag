#version 450
#extension GL_ARB_separate_shader_objects : enable

//-----------------------------------------------------------------------------
// [SECTION] specialication constants
//-----------------------------------------------------------------------------

layout(constant_id = 0) const int iMeshVariantFlags = 0;
layout(constant_id = 1) const int iDataStride = 0;

//-----------------------------------------------------------------------------
// [SECTION] bind group 0
//-----------------------------------------------------------------------------

layout(set = 0, binding = 0) uniform _plGlobalInfo
{
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

layout(set = 1, binding = 0) uniform _plObjectInfo
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

// output
layout(location = 0) out vec4 outColor;

void
main() 
{
    outColor = tObjectInfo.tColor;
}