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

const float GAMMA = 2.2;
const float INV_GAMMA = 1.0 / GAMMA;

// linear to sRGB approximation
// see http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
vec3 linearTosRGB(vec3 color)
{
    return pow(color, vec3(INV_GAMMA));
}

layout(location = 0) out vec4 outColor;

// output
layout(location = 0) in struct plShaderIn {
    vec3 tPosition;
    vec4 tWorldPosition;
    vec2 tUV[2];
    vec4 tColor;
    vec3 tWorldNormal;
    mat3 tTBN;
} tShaderIn;

struct NormalInfo {
    vec3 ng;   // Geometry normal
    vec3 t;    // Geometry tangent
    vec3 b;    // Geometry bitangent
    vec3 n;    // Shading normal
    vec3 ntex; // Normal from texture, scaling is accounted for.
};

NormalInfo pl_get_normal_info()
{
    vec2 UV = tShaderIn.tUV[0];
    vec2 uv_dx = dFdx(UV);
    vec2 uv_dy = dFdy(UV);

    if (length(uv_dx) <= 1e-2) {
      uv_dx = vec2(1.0, 0.0);
    }

    if (length(uv_dy) <= 1e-2) {
      uv_dy = vec2(0.0, 1.0);
    }

    vec3 t_ = (uv_dy.t * dFdx(tShaderIn.tPosition) - uv_dx.t * dFdy(tShaderIn.tPosition)) /
        (uv_dx.s * uv_dy.t - uv_dy.s * uv_dx.t);

    vec3 n, t, b, ng;

    // Compute geometrical TBN:
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_NORMAL))
    {

        if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TANGENT))
        {
            // Trivial TBN computation, present as vertex attribute.
            // Normalize eigenvectors as matrix is linearly interpolated.
            t = normalize(tShaderIn.tTBN[0]);
            b = normalize(tShaderIn.tTBN[1]);
            ng = normalize(tShaderIn.tTBN[2]);
        }
        else
        {
            // Normals are either present as vertex attributes or approximated.
            ng = normalize(tShaderIn.tWorldNormal);
            t = normalize(t_ - ng * dot(ng, t_));
            b = cross(ng, t);
        }
    }
    else
    {
        ng = normalize(cross(dFdx(tShaderIn.tPosition), dFdy(tShaderIn.tPosition)));
        t = normalize(t_ - ng * dot(ng, t_));
        b = cross(ng, t);
    }


    // For a back-facing surface, the tangential basis vectors are negated.
    if (gl_FrontFacing == false)
    {
        t *= -1.0;
        b *= -1.0;
        ng *= -1.0;
    }

    // Compute normals:
    NormalInfo info;
    info.ng = ng;
    if(bool(PL_HAS_NORMAL_MAP)) 
    {
        info.ntex = texture(tNormalSampler, UV).rgb * 2.0 - vec3(1.0);
        // info.ntex *= vec3(0.2, 0.2, 1.0);
        // info.ntex *= vec3(u_NormalScale, u_NormalScale, 1.0);
        info.ntex = normalize(info.ntex);
        info.n = normalize(mat3(t, b, ng) * info.ntex);
    }
    else
    {
        info.n = ng;
    }
    info.t = t;
    info.b = b;
    return info;
}

vec4 getBaseColor(vec4 u_ColorFactor)
{
    vec4 baseColor = u_ColorFactor;

    if(bool(PL_HAS_BASE_COLOR_MAP))
    {
        baseColor *= texture(tBaseColorSampler, tShaderIn.tUV[0]);
    }
    return baseColor;
}

void main() 
{
    vec4 tBaseColor = getBaseColor(tMaterialInfo.atMaterials[tObjectInfo.iMaterialIndex].tColor);
    vec3 tSunlightColor = vec3(1.0, 1.0, 1.0);
    NormalInfo tNormalInfo = pl_get_normal_info();
    vec3 tSunLightDirection = vec3(-1.0, -1.0, -1.0);
    float fDiffuseIntensity = max(0.0, dot(normalize(tNormalInfo.n), -normalize(tSunLightDirection)));
    outColor = tBaseColor * vec4(tSunlightColor * (0.05 + fDiffuseIntensity), 1.0);

    outColor = vec4(linearTosRGB(outColor.rgb), tBaseColor.a);
}