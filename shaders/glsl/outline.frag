#version 450
#extension GL_ARB_separate_shader_objects : enable

//-----------------------------------------------------------------------------
// [SECTION] shader input/output
//-----------------------------------------------------------------------------

// output
layout(location = 0) out vec4 outColor;

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

layout(constant_id = 0) const int MeshVariantFlags = 0;
layout(constant_id = 1) const int VertexStride = 0;
layout(constant_id = 2) const int ShaderTextureFlags = 0;

//-----------------------------------------------------------------------------
// [SECTION] global
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

//-----------------------------------------------------------------------------
// [SECTION] material
//-----------------------------------------------------------------------------

layout(set = 1, binding = 0) uniform _plMaterialInfo
{
    vec4 tAlbedo;
} tMaterialInfo;

//-----------------------------------------------------------------------------
// [SECTION] object
//-----------------------------------------------------------------------------

layout(set = 2, binding = 0) uniform _plObjectInfo
{
    mat4 tModel;
    uint uVertexOffset;
    // ivec3 _unused0;
} tObjectInfo;

layout(set = 1, binding = 1) uniform sampler2D tColorSampler;
layout(set = 1, binding = 2) uniform sampler2D tNormalSampler;
layout(set = 1, binding = 3) uniform sampler2D tEmissiveSampler;

void main() 
{
    outColor = tMaterialInfo.tAlbedo;
}