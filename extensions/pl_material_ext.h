/*
   pl_material_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] apis
// [SECTION] includes
// [SECTION] defines
// [SECTION] forward declarations & basic types
// [SECTION] public api structs
// [SECTION] enums
// [SECTION] structs
// [SECTION] components
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*

    Implementation:
        The provided implementation of this extension depends on the following
        APIs being available:

        * plEcsI (v1.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_MATERIAL_EXT_H
#define PL_MATERIAL_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plMaterialI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_ecs_ext.inl"      // plEntity
#include "pl_resource_ext.inl" // plResourceHandle
#include "pl_math.h"           // plVec3, plMat4

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_MAX_PATH_LENGTH
    #define PL_MAX_PATH_LENGTH 1024
#endif

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// ecs components
typedef struct _plMaterialComponent plMaterialComponent;

// enums & flags
typedef int plShaderType;
typedef int plMaterialFlags;
typedef int plTextureSlot;

// external
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h
typedef struct _plGpuMaterial      plGpuMaterial;      // pl_shader_interop_renderer.h

// external enums
typedef int plBlendMode; // pl_graphics_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plMaterialI
{
    // system setup/shutdown/etc
    void (*register_ecs_system)(void);

    // do NOT store out parameter; use it immediately
    plEntity (*create)(plComponentLibrary*, const char* name, plMaterialComponent**);

    // ecs types
    plEcsTypeKey (*get_ecs_type_key)(void);
} plMaterialI;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plTextureSlot
{
    PL_TEXTURE_SLOT_BASE_COLOR_MAP = 0,
    PL_TEXTURE_SLOT_NORMAL_MAP,
    PL_TEXTURE_SLOT_EMISSIVE_MAP,
    PL_TEXTURE_SLOT_OCCLUSION_MAP,
    PL_TEXTURE_SLOT_METAL_ROUGHNESS_MAP,
    PL_TEXTURE_SLOT_CLEARCOAT_MAP,
    PL_TEXTURE_SLOT_CLEARCOAT_ROUGHNESS_MAP,
    PL_TEXTURE_SLOT_CLEARCOAT_NORMAL_MAP,
    
    PL_TEXTURE_SLOT_COUNT
};

enum _plShaderType
{
    PL_SHADER_TYPE_PBR,
    PL_SHADER_TYPE_PBR_CLEARCOAT,
    PL_SHADER_TYPE_CUSTOM,
    
    PL_SHADER_TYPE_COUNT
};

enum _plMaterialFlags
{
    PL_MATERIAL_FLAG_NONE                = 0,
    PL_MATERIAL_FLAG_DOUBLE_SIDED        = 1 << 0,
    PL_MATERIAL_FLAG_OUTLINE             = 1 << 1,
    PL_MATERIAL_FLAG_CAST_SHADOW         = 1 << 2,
    PL_MATERIAL_FLAG_CAST_RECEIVE_SHADOW = 1 << 3
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plTextureMap
{
    char             acName[PL_MAX_PATH_LENGTH];
    plResourceHandle tResource;
    uint32_t         uUVSet;
} plTextureMap;

//-----------------------------------------------------------------------------
// [SECTION] components
//-----------------------------------------------------------------------------

typedef struct _plMaterialComponent
{
    plMaterialFlags tFlags;              // default: PL_MATERIAL_FLAG_CAST_SHADOW | PL_MATERIAL_FLAG_CAST_RECEIVE_SHADOW
    plShaderType    tShaderType;         // default: PL_SHADER_TYPE_PBR
    plBlendMode     tBlendMode;          // default: PL_BLEND_MODE_OPAQUE
    plVec4          tBaseColor;          // default: {1.0f, 1.0f, 1.0f, 1.0f}
    plVec4          tEmissiveColor;      // default: {0.0f, 0.0f, 0.0f, 0.0f}
    float           fAlphaCutoff;        // default: 0.5f
    float           fRoughness;          // default: 1.0f
    float           fMetalness;          // default: 1.0f
    float           fClearcoat;          // default: 0.0f
    float           fClearcoatRoughness; // default: 0.0f
    plTextureMap    atTextureMaps[PL_TEXTURE_SLOT_COUNT];
} plMaterialComponent;

#endif // PL_MATERIAL_EXT_H