#version 450
#extension GL_ARB_separate_shader_objects : enable

struct tMaterial
{
    vec4 tColor;
};

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

layout(set = 0, binding = 2) readonly buffer plMaterialInfo
{
    tMaterial atMaterials[];
} tMaterialInfo;

layout(set = 0, binding = 3)  uniform sampler tDefaultSampler;

layout(set = 2, binding = 0)  uniform texture2D tSkinningSampler;

layout(set = 3, binding = 0) uniform _plObjectInfo
{
    int iDataOffset;
    int iVertexOffset;
} tObjectInfo;

// input
layout(location = 0) in vec3 inPos;

// output
layout(location = 0) out struct plShaderOut {
    vec2 tUV;
} tShaderOut;

void main() 
{

    vec4 inPosition  = vec4(inPos, 1.0);
    vec2 inTexCoord0 = vec2(0.0, 0.0);
    int iCurrentAttribute = 0;

    const uint iVertexDataOffset = 1 * (gl_VertexIndex - tObjectInfo.iVertexOffset) + tObjectInfo.iDataOffset;
    inTexCoord0 = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;  iCurrentAttribute++;
    gl_Position = inPosition;
    tShaderOut.tUV = inTexCoord0;
}