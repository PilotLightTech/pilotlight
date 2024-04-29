#version 450
#extension GL_ARB_separate_shader_objects : enable

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

layout(std140, set = 0, binding = 1) readonly buffer _tVertexBuffer{ vec4 atVertexData[]; } tVertexBuffer;

layout(set = 0, binding = 3)  uniform sampler tDefaultSampler;
layout(set = 0, binding = 4)  uniform sampler tEnvSampler;
layout (set = 0, binding = 5) uniform textureCube u_LambertianEnvSampler;
layout (set = 0, binding = 6) uniform textureCube u_GGXEnvSampler;
layout (set = 0, binding = 7) uniform texture2D u_GGXLUT;

//-----------------------------------------------------------------------------
// [SECTION] bind group 1
//-----------------------------------------------------------------------------

layout(set = 1, binding = 0) uniform textureCube samplerCubeMap;

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(set = 2, binding = 0) uniform _plObjectInfo { mat4 tModel;} tObjectInfo;

//-----------------------------------------------------------------------------
// [SECTION] input
//-----------------------------------------------------------------------------

layout(location = 0) in vec3 inPos;

//-----------------------------------------------------------------------------
// [SECTION] output
//-----------------------------------------------------------------------------

layout(location = 0) out struct plShaderOut {
    vec3 tPosition;
    vec3 tWorldPosition;
} tShaderOut;

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void
main() 
{
    gl_Position = tGlobalInfo.tCameraProjection * tGlobalInfo.tCameraView * tObjectInfo.tModel * vec4(inPos, 1.0);
    gl_Position.w = gl_Position.z;
    tShaderOut.tPosition = gl_Position.xyz;
    tShaderOut.tWorldPosition = inPos;
}