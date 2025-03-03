#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "defines.glsl"
#include "material.glsl"

//-----------------------------------------------------------------------------
// [SECTION] specialication constants
//-----------------------------------------------------------------------------

layout(constant_id = 0) const int iMeshVariantFlags = 0;
layout(constant_id = 1) const int iDataStride = 0;
layout(constant_id = 2) const int iTextureMappingFlags = 0;
layout(constant_id = 3) const int iMaterialFlags = 0;

//-----------------------------------------------------------------------------
// [SECTION] bind group 0
//-----------------------------------------------------------------------------

layout(std140, set = 0, binding = 0) readonly buffer _tVertexBuffer
{
	vec4 atVertexData[];
} tVertexBuffer;

layout(std140, set = 0, binding = 1) readonly buffer _tTransformBuffer
{
	mat4 atTransform[];
} tTransformBuffer;

layout(set = 0, binding = 2) readonly buffer plMaterialInfo
{
    tMaterial atMaterials[];
} tMaterialInfo;

layout(set = 0, binding = 3)  uniform sampler tDefaultSampler;
layout(set = 0, binding = 4)  uniform sampler tEnvSampler;
layout(set = 0, binding = 5)  uniform texture2D at2DTextures[4096];
layout(set = 0, binding = 4101)  uniform textureCube atCubeTextures[4096];

//-----------------------------------------------------------------------------
// [SECTION] bind group 1
//-----------------------------------------------------------------------------

layout(set = 1, binding = 0) readonly buffer _plCameraInfo
{
    mat4 atCameraProjs[];
} tCameraInfo;

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(std140, set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    int  iIndex;
    int  iDataOffset;
    int  iVertexOffset;
    int  iMaterialIndex;
    
    uint uTransformIndex;
} tObjectInfo;

//-----------------------------------------------------------------------------
// [SECTION] input & output
//-----------------------------------------------------------------------------

// output
layout(location = 0) in struct plShaderIn {
    vec2 tUV[8];
} tShaderIn;

//-----------------------------------------------------------------------------
// [SECTION] helpers
//-----------------------------------------------------------------------------

vec4 getBaseColor(vec4 u_ColorFactor, int iUVSet)
{
    vec4 baseColor = vec4(1);

    if(bool(iMaterialFlags & PL_MATERIAL_METALLICROUGHNESS))
    {
        // baseColor = u_BaseColorFactor;
        baseColor = u_ColorFactor;
    }

    if(bool(iMaterialFlags & PL_MATERIAL_METALLICROUGHNESS) && bool(iTextureMappingFlags & PL_HAS_BASE_COLOR_MAP))
    {
        tMaterial material = tMaterialInfo.atMaterials[tObjectInfo.iMaterialIndex];
        baseColor *= texture(sampler2D(at2DTextures[nonuniformEXT(material.iBaseColorTexIdx)], tDefaultSampler), tShaderIn.tUV[iUVSet]);
    }
    return baseColor;
}

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void main() 
{
    tMaterial material = tMaterialInfo.atMaterials[tObjectInfo.iMaterialIndex];
    vec4 tBaseColor = getBaseColor(material.u_BaseColorFactor, material.BaseColorUVSet);

    if(tBaseColor.a <  material.u_AlphaCutoff)
    {
        discard;
    }
}