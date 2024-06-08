
// math
const float M_PI = 3.141592653589793;
const float GAMMA = 2.2;
const float INV_GAMMA = 1.0 / GAMMA;

// iMeshVariantFlags
const int PL_MESH_FORMAT_FLAG_NONE           = 0;
const int PL_MESH_FORMAT_FLAG_HAS_POSITION   = 1 << 0;
const int PL_MESH_FORMAT_FLAG_HAS_NORMAL     = 1 << 1;
const int PL_MESH_FORMAT_FLAG_HAS_TANGENT    = 1 << 2;
const int PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0 = 1 << 3;
const int PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1 = 1 << 4;
const int PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_2 = 1 << 5;
const int PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_3 = 1 << 6;
const int PL_MESH_FORMAT_FLAG_HAS_COLOR_0    = 1 << 7;
const int PL_MESH_FORMAT_FLAG_HAS_COLOR_1    = 1 << 8;
const int PL_MESH_FORMAT_FLAG_HAS_JOINTS_0   = 1 << 9;
const int PL_MESH_FORMAT_FLAG_HAS_JOINTS_1   = 1 << 10;
const int PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0  = 1 << 11;
const int PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1  = 1 << 12;

// iTextureMappingFlags
const int PL_HAS_BASE_COLOR_MAP            = 1 << 0;
const int PL_HAS_NORMAL_MAP                = 1 << 1;
const int PL_HAS_EMISSIVE_MAP              = 1 << 2;
const int PL_HAS_OCCLUSION_MAP             = 1 << 3;
const int PL_HAS_METALLIC_ROUGHNESS_MAP    = 1 << 4;

// iMaterialFlags
const int PL_MATERIAL_METALLICROUGHNESS = 1 << 0;

// iRenderingFlags
const int PL_RENDERING_FLAG_USE_PUNCTUAL = 1 << 0;
const int PL_RENDERING_FLAG_USE_IBL      = 1 << 1;

// lights
const int PL_LIGHT_TYPE_DIRECTIONAL = 0;
const int PL_LIGHT_TYPE_POINT = 1;