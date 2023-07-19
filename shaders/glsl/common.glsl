
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

const int PL_TEXTURE_HAS_BASE_COLOR   = 1 << 0;
const int PL_TEXTURE_HAS_NORMAL       = 1 << 1;
const int PL_TEXTURE_HAS_EMISSIVE     = 1 << 2;

layout(constant_id = 0) const int MeshVariantFlags = PL_MESH_FORMAT_FLAG_NONE;
layout(constant_id = 1) const int VertexStride = 0;
layout(constant_id = 2) const int ShaderTextureFlags = 0;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

struct plMaterialInfo
{
    vec4 tAlbedo;
};

//-----------------------------------------------------------------------------
// [SECTION] global
//-----------------------------------------------------------------------------

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

layout(std140, set = 0, binding = 1) readonly buffer _tVertexBuffer
{
	vec4 atVertexData[];
} tVertexBuffer;

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
// [SECTION] per object
//-----------------------------------------------------------------------------

layout(set = 2, binding = 0) uniform _plObjectInfo
{
    mat4  tModel;
    uint  uMaterialIndex;
    uint  uVertexDataOffset;
    uint  uVertexPosOffset;
} tObjectInfo;