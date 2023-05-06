#version 450
#extension GL_ARB_separate_shader_objects : enable

//-----------------------------------------------------------------------------
// [SECTION] shader input/output
//-----------------------------------------------------------------------------

// input
layout(location = 0) in struct plShaderOut {
    vec3 tWorldNormal;
    vec2 tUV;
} tShaderIn;

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

struct plMaterialInfo
{
    vec4 tAlbedo;
};

layout(std140, set = 0, binding = 2) readonly buffer _plMaterialBuffer
{
	plMaterialInfo atMaterialData[];
} tMaterialBuffer;

//-----------------------------------------------------------------------------
// [SECTION] material
//-----------------------------------------------------------------------------

layout(set = 1, binding = 1) uniform sampler2D tColorSampler;
layout(set = 1, binding = 2) uniform sampler2D tNormalSampler;
layout(set = 1, binding = 3) uniform sampler2D tEmissiveSampler;

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


float clampedDot(vec3 x, vec3 y)
{
    return clamp(dot(x, y), 0.0, 1.0);
}

void main() 
{
    
    const vec3 n = normalize(tShaderIn.tWorldNormal);

    const vec3 tLightDir0 = normalize(vec3(0.0, -1.0, -1.0));
    const vec3 tEyePos = normalize(-tGlobalInfo.tCameraPos.xyz);
    const vec3 tReflected = normalize(reflect(-tLightDir0, n));
    const vec4 tLightColor = vec4(1.0, 1.0, 1.0, 1.0);
    const vec4 tDiffuseColor = tLightColor * clampedDot(n, -tLightDir0);
    const vec4 tMaterialColor = texture(tColorSampler, tShaderIn.tUV);

    outColor = (tGlobalInfo.tAmbientColor + tDiffuseColor) * tMaterialColor;
    outColor.a = tMaterialColor.a;
    if(tMaterialColor.a < 0.01)
    {
        discard;
    }

}