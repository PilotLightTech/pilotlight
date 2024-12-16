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
    uint uLambertianEnvSampler;
    uint uGGXEnvSampler;
    uint uGGXLUT;
    uint _uUnUsed;
} tGlobalInfo;

layout(set = 0, binding = 1)  uniform sampler tDefaultSampler;

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

layout(location = 0) in struct plShaderOut {
    vec3 tWorldPosition;
} tShaderIn;

//-----------------------------------------------------------------------------
// [SECTION] output
//-----------------------------------------------------------------------------

layout(location = 0) out vec4 outColor;

//-----------------------------------------------------------------------------
// [SECTION] helpers
//-----------------------------------------------------------------------------

const float GAMMA = 2.2;
const float INV_GAMMA = 1.0 / GAMMA;



//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void
main() 
{
    vec3 tVectorOut = normalize(tShaderIn.tWorldPosition);
    outColor = vec4(texture(samplerCube(samplerCubeMap, tDefaultSampler), tVectorOut).rgb, 1.0);
}