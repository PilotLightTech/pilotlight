#version 450
#extension GL_ARB_separate_shader_objects : enable

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

const int PL_MESH_FORMAT_FLAG_NONE           = 0;
const int PL_MESH_FORMAT_FLAG_HAS_POSITION   = 1 << 0;
const int PL_MESH_FORMAT_FLAG_HAS_NORMAL     = 1 << 1;
const int PL_MESH_FORMAT_FLAG_HAS_TANGENT    = 1 << 2;
const int PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0 = 1 << 3;
const int PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1 = 1 << 4;
const int PL_MESH_FORMAT_FLAG_HAS_COLOR_0    = 1 << 5;
const int PL_MESH_FORMAT_FLAG_HAS_COLOR_1    = 1 << 6;
const int PL_MESH_FORMAT_FLAG_HAS_JOINTS_0   = 1 << 7;
const int PL_MESH_FORMAT_FLAG_HAS_JOINTS_1   = 1 << 8;
const int PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0  = 1 << 9;
const int PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1  = 1 << 10;

layout(constant_id = 0) const int MeshVariantFlags = PL_MESH_FORMAT_FLAG_NONE;
layout(constant_id = 1) const int PL_DATA_STRIDE = 0;
layout(constant_id = 2) const int PL_HAS_BASE_COLOR_MAP = 0;
layout(constant_id = 3) const int PL_HAS_NORMAL_MAP = 0;
layout(constant_id = 4) const int PL_USE_SKINNING = 0;

//-----------------------------------------------------------------------------
// [SECTION] global
//-----------------------------------------------------------------------------

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

layout(set = 1, binding = 0)  uniform sampler2D tBaseColorSampler;
layout(set = 1, binding = 1)  uniform sampler2D tNormalSampler;

layout(set = 2, binding = 0)  uniform sampler2D tSkinningSampler;

layout(set = 3, binding = 0) uniform _plObjectInfo
{
    int  iDataOffset;
    int  iVertexOffset;
    int  iMaterialIndex;
    mat4 tModel;
} tObjectInfo;

// input
layout(location = 0) in vec3 inPos;

// output
layout(location = 0) out struct plShaderOut {
    vec3 tPosition;
    vec4 tWorldPosition;
    vec2 tUV[2];
    vec4 tColor;
    vec3 tWorldNormal;
    mat3 tTBN;
} tShaderOut;

mat4 get_matrix_from_texture(sampler2D s, int index)
{
    mat4 result = mat4(1);
    int texSize = textureSize(s, 0)[0];
    int pixelIndex = index * 4;
    for (int i = 0; i < 4; ++i)
    {
        int x = (pixelIndex + i) % texSize;
        //Rounding mode of integers is undefined:
        //https://www.khronos.org/registry/OpenGL/specs/es/3.0/GLSL_ES_Specification_3.00.pdf (section 12.33)
        int y = (pixelIndex + i - x) / texSize; 
        result[i] = texelFetch(s, ivec2(x,y), 0);
    }
    return result;
}

mat4 get_skinning_matrix(vec4 inJoints0, vec4 inWeights0)
{
    mat4 skin = mat4(0);

    skin +=
        inWeights0.x * get_matrix_from_texture(tSkinningSampler, int(inJoints0.x) * 2) +
        inWeights0.y * get_matrix_from_texture(tSkinningSampler, int(inJoints0.y) * 2) +
        inWeights0.z * get_matrix_from_texture(tSkinningSampler, int(inJoints0.z) * 2) +
        inWeights0.w * get_matrix_from_texture(tSkinningSampler, int(inJoints0.w) * 2);

    if (skin == mat4(0)) { 
        return mat4(1); 
    }
    return skin;
}

vec4 get_position(vec4 inJoints0, vec4 inWeights0)
{
    vec4 pos = vec4(inPos, 1.0);

    if(bool(PL_USE_SKINNING))
    {
        pos = get_skinning_matrix(inJoints0, inWeights0) * pos;
    }

    return pos;
}

vec3 get_normal(vec3 inNormal, vec4 inJoints0, vec4 inWeights0)
{
    vec3 tNormal = inNormal;
    if(bool(PL_USE_SKINNING))
    {
        tNormal = mat3(get_skinning_matrix(inJoints0, inWeights0)) * tNormal;
    }
    return normalize(tNormal);
}

vec3 get_tangent(vec4 inTangent, vec4 inJoints0, vec4 inWeights0)
{
    vec3 tTangent = inTangent.xyz;
    if(bool(PL_USE_SKINNING))
    {
        tTangent = mat3(get_skinning_matrix(inJoints0, inWeights0)) * tTangent;
    }
    return normalize(tTangent);
}

void main() 
{

    vec4 inPosition  = vec4(inPos, 1.0);
    vec3 inNormal    = vec3(0.0, 0.0, 0.0);
    vec4 inTangent   = vec4(0.0, 0.0, 0.0, 0.0);
    vec2 inTexCoord0 = vec2(0.0, 0.0);
    vec2 inTexCoord1 = vec2(0.0, 0.0);
    vec4 inColor0    = vec4(1.0, 1.0, 1.0, 1.0);
    vec4 inColor1    = vec4(0.0, 0.0, 0.0, 0.0);
    vec4 inJoints0   = vec4(0.0, 0.0, 0.0, 0.0);
    vec4 inJoints1   = vec4(0.0, 0.0, 0.0, 0.0);
    vec4 inWeights0  = vec4(0.0, 0.0, 0.0, 0.0);
    vec4 inWeights1  = vec4(0.0, 0.0, 0.0, 0.0);
    int iCurrentAttribute = 0;
    
    // offset = offset into current mesh + offset into global buffer
    const uint iVertexDataOffset = PL_DATA_STRIDE * (gl_VertexIndex - tObjectInfo.iVertexOffset) + tObjectInfo.iDataOffset;

    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_POSITION))  { inPosition.xyz = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute].xyz; iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_NORMAL))    { inNormal       = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute].xyz; iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TANGENT))   { inTangent      = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0)){ inTexCoord0    = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;  iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1)){ inTexCoord1    = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;  iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_COLOR_0))   { inColor0       = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_COLOR_1))   { inColor1       = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_JOINTS_0))  { inJoints0      = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_JOINTS_1))  { inJoints1      = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0)) { inWeights0     = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1)) { inWeights1     = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}

    tShaderOut.tWorldNormal = mat3(tObjectInfo.tModel) * get_normal(inNormal, inJoints0, inWeights0);
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_NORMAL))
    {

        if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TANGENT))
        {
            vec3 tangent = get_tangent(inTangent, inJoints0, inWeights0);
            vec3 WorldTangent = mat3(tObjectInfo.tModel) * tangent;
            vec3 WorldBitangent = cross(get_normal(inNormal, inJoints0, inWeights0), tangent) * inTangent.w;
            WorldBitangent = mat3(tObjectInfo.tModel) * WorldBitangent;
            tShaderOut.tTBN = mat3(WorldTangent, WorldBitangent, tShaderOut.tWorldNormal);
        }
    }

    vec4 pos = tObjectInfo.tModel * get_position(inJoints0, inWeights0);
    tShaderOut.tPosition = pos.xyz / pos.w;
    gl_Position = tGlobalInfo.tCameraViewProjection * pos;
    tShaderOut.tUV[0] = inTexCoord0;
    tShaderOut.tWorldPosition = gl_Position / gl_Position.w;
    tShaderOut.tColor = inColor0;
}