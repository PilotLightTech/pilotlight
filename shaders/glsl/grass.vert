#version 450
#extension GL_ARB_separate_shader_objects : enable

//-----------------------------------------------------------------------------
// [SECTION] shader input/output
//-----------------------------------------------------------------------------

// input
layout(location = 0) in vec3 inPos;

// output
layout(location = 0) out struct plShaderOut {
    vec3 tWorldNormal;
    vec2 tUV;
} tShaderOut;

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

layout(std140, set = 0, binding = 1) readonly buffer _tVertexBuffer
{
	vec4 atVertexData[];
} tVertexBuffer;

//-----------------------------------------------------------------------------
// [SECTION] object
//-----------------------------------------------------------------------------

layout(set = 2, binding = 0) uniform _plObjectInfo
{
    mat4  tModel;
    uint  uMaterialIndex;
    uint  uVertexDataOffset;
    uint  uVertexOffset;
} tObjectInfo;


//-----------------------------------------------------------------------------
// [SECTION] specialization constants
//-----------------------------------------------------------------------------

layout(constant_id = 0) const int MeshVariantFlags = 0;
layout(constant_id = 1) const int VertexStride = 0;
layout(constant_id = 2) const int ShaderTextureFlags = 0;

void main() 
{

    vec3 inPosition  = inPos;
    vec3 inNormal    = tVertexBuffer.atVertexData[VertexStride * (gl_VertexIndex - tObjectInfo.uVertexOffset) + tObjectInfo.uVertexDataOffset].xyz; 
    vec3 inTangent   = tVertexBuffer.atVertexData[VertexStride * (gl_VertexIndex - tObjectInfo.uVertexOffset) + tObjectInfo.uVertexDataOffset + 1].xyz; 
    vec2 inTexCoord0 = tVertexBuffer.atVertexData[VertexStride * (gl_VertexIndex - tObjectInfo.uVertexOffset) + tObjectInfo.uVertexDataOffset + 2].xy;

    // animate upper vertices and normals only
    if(inTexCoord0.y < 0.1)
    {
        const vec3 tTranslation = vec3(0.25 * sin(tGlobalInfo.fTime), 0.0, 0.15 * cos(tGlobalInfo.fTime * 2.0));
        inPosition = inPosition + tTranslation;
        inNormal = normalize(inNormal * 1.0 + tTranslation);
    }

    const mat4 tMVP = tGlobalInfo.tCameraViewProj * tObjectInfo.tModel;
    gl_Position = tMVP * vec4(inPosition, 1.0);
    tShaderOut.tUV = inTexCoord0;
    tShaderOut.tWorldNormal = normalize(tObjectInfo.tModel * vec4(inNormal, 1.0)).xyz;
}