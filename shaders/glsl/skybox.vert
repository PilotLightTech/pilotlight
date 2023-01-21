#version 450
#extension GL_ARB_separate_shader_objects : enable

//-----------------------------------------------------------------------------
// [SECTION] shader input/output
//-----------------------------------------------------------------------------

// input
layout(location = 0) in vec3 inPos;

// output
layout(location = 0) out struct plShaderOut {
    vec3 tPosition;
    vec3 tWorldPosition;
} tShaderOut;

//-----------------------------------------------------------------------------
// [SECTION] object
//-----------------------------------------------------------------------------

layout(set = 0, binding = 0) uniform _plGlobalInfo
{
    vec4 tAmbientColor;

    // camera info
    vec4 tCameraPos;
    mat4 tCameraView;
    mat4 tCameraViewProj;

    // misc
    float fTime;

} tGlobalInfo;

//-------------------d----------------------------------------------------------
// [SECTION] specialization constants
//-----------------------------------------------------------------------------

layout(constant_id = 0) const int MeshVariantFlags = 0;
layout(constant_id = 1) const int VertexStride = 0;
layout(constant_id = 2) const int ShaderTextureFlags = 0;

void main() 
{
    gl_Position = tGlobalInfo.tCameraViewProj * vec4(inPos, 1.0);
    gl_Position.w = gl_Position.z;
    tShaderOut.tPosition = gl_Position.xyz;
    tShaderOut.tWorldPosition = inPos;
}