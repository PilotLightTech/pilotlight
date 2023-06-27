#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform _plGlobalInfo
{
    vec4 tAmbientColor;

    // misc
    float fTime;

    // light info
    vec4 tLightColor;
    vec4 tLightPos;

    // camera info
    vec4 tCameraPos;
    mat4 tCameraView;
    mat4 tCameraViewProj;

} tGlobalInfo;

struct plPickInfo
{
    vec4 tColor;
};

layout(std140, set = 0, binding = 1) readonly buffer _plMaterialBuffer
{
	plPickInfo atMaterialData[];
} tMaterialBuffer;

layout(set = 1, binding = 0) uniform _plObjectInfo
{
    mat4  tModel;
    uint  uMaterialIndex;
    uint  uVertexDataOffset;
    uint  uVertexOffset;
} tObjectInfo;

// input
layout(location = 0) in vec3 inPos;

// output
layout(location = 0) out struct plShaderOut {
    vec4 tColor;
} tShaderOut;

void main() 
{
    const mat4 tMVP = tGlobalInfo.tCameraViewProj * tObjectInfo.tModel;
    gl_Position = tMVP * vec4(inPos, 1.0);
    tShaderOut.tColor = tMaterialBuffer.atMaterialData[tObjectInfo.uMaterialIndex].tColor;
}