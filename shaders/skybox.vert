#version 450
#extension GL_ARB_separate_shader_objects : enable

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

layout(std140, set = 0, binding = 1) readonly buffer _tVertexBuffer0{ vec4 atVertexData[]; } tVertexBuffer0;
layout(std140, set = 0, binding = 2) readonly buffer _tVertexBuffer1{ vec4 atVertexData[]; } tVertexBuffer1;

layout(set = 0, binding = 3)  uniform sampler tDefaultSampler;

//-----------------------------------------------------------------------------
// [SECTION] bind group 1
//-----------------------------------------------------------------------------

layout(set = 1, binding = 0) uniform textureCube samplerCubeMap;

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA { mat4 tModel;} tObjectInfo;

//-----------------------------------------------------------------------------
// [SECTION] input
//-----------------------------------------------------------------------------

layout(location = 0) in vec3 inPos;

//-----------------------------------------------------------------------------
// [SECTION] output
//-----------------------------------------------------------------------------

layout(location = 0) out struct plShaderOut {
    vec3 tWorldPosition;
} tShaderOut;

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void
main() 
{
    gl_Position = tGlobalInfo.tCameraProjection * tGlobalInfo.tCameraView * tObjectInfo.tModel * vec4(inPos, 1.0);
    gl_Position.z = 0.0;
    // gl_Position.w = gl_Position.z; uncomment if not reverse z
    tShaderOut.tWorldPosition = inPos;
    tShaderOut.tWorldPosition.z = -inPos.z;
}