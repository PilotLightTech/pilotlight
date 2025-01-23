#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "material.glsl"

layout(std140, set = 0, binding = 0) readonly buffer _tVertexBuffer
{
	vec4 atVertexData[];
} tVertexBuffer;

layout(set = 0, binding = 1) readonly buffer plMaterialInfo
{
    tMaterial atMaterials[];
} tMaterialInfo;

layout(set = 0, binding = 2)  uniform sampler tDefaultSampler;
layout(set = 0, binding = 3)  uniform sampler tEnvSampler;

layout(set = 0, binding = 4)  uniform texture2D at2DTextures[4096];
layout(set = 0, binding = 4100)  uniform textureCube atCubeTextures[4096];

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    uint uGlobalIndex;
} tObjectInfo;

// input
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;

// output
layout(location = 0) out struct plShaderOut {
    vec2 tUV;
} tShaderOut;

void main() 
{

    vec4 inPosition  = vec4(inPos, 0.0, 1.0);
    gl_Position = inPosition;
    tShaderOut.tUV = inUV;
}