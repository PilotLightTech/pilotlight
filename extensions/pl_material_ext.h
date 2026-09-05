/*
   pl_material_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] apis
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] enums
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*

    Implementation:
        The provided implementation of this extension depends on the following
        APIs being available:

        * plVfsI   (v2.1)
        * plAssetI (v1.x)
        * plJsonI  (v1.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_MATERIAL_EXT_H
#define PL_MATERIAL_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plMaterialI_version {0, 3, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h> // bool
#include "pl.inc"
#include "pl_asset_ext.inl" // plAssetHandle
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plMaterial                    plMaterial;
typedef struct _plMaterialTexture             plMaterialTexture;
typedef struct _plMaterialClearcoat           plMaterialClearcoat;
typedef struct _plMaterialSheen               plMaterialSheen;
typedef struct _plMaterialIridescence         plMaterialIridescence;
typedef struct _plMaterialDispersion          plMaterialDispersion;
typedef struct _plMaterialDiffuseTransmission plMaterialDiffuseTransmission;
typedef struct _plMaterialTransmission        plMaterialTransmission;
typedef struct _plMaterialVolume              plMaterialVolume;
typedef struct _plMaterialAnisotropy          plMaterialAnisotropy;

// enums & flags
typedef int plMaterialModel;       // -> enum _plMaterialModel       // Enum: (PL_MATERIAL_MODEL_XXXX)
typedef int plMaterialFlags;       // -> enum _plMaterialFlags       // Flag: (PL_MATERIAL_FLAG_XXXX)
typedef int plMaterialTextureSlot; // -> enum _plMaterialTextureSlot // Enum: (PL_MATERIAL_TEXTURE_SLOT_XXXX)
typedef int plMaterialAlphaMode;   // -> enum _plMaterialAlphaMode   // Enum: (PL_MATERIAL_ALPHA_MODE_XXXX)

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_material_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_material_ext(plApiRegistryI*, bool reload);

// basic api
PL_API void pl_material_init(plMaterial*); // constructor

// asset system
PL_API void           pl_material_register_asset_types(void);
PL_API plAssetTypeKey pl_material_get_asset_type_key  (void);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plMaterialI
{
    // basic
    void (*init)(plMaterial*); // constructor

    // asset
    void           (*register_asset_types)(void);
    plAssetTypeKey (*get_asset_type_key)  (void);
} plMaterialI;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plMaterialAlphaMode
{
    PL_MATERIAL_ALPHA_MODE_OPAQUE = 0,
    PL_MATERIAL_ALPHA_MODE_MASK,
    PL_MATERIAL_ALPHA_MODE_BLEND
};

enum _plMaterialTextureSlot
{
    PL_MATERIAL_TEXTURE_SLOT_BASE_COLOR = 0,
    PL_MATERIAL_TEXTURE_SLOT_NORMAL,
    PL_MATERIAL_TEXTURE_SLOT_EMISSIVE,
    PL_MATERIAL_TEXTURE_SLOT_OCCLUSION,
    PL_MATERIAL_TEXTURE_SLOT_METAL_ROUGHNESS,
    PL_MATERIAL_TEXTURE_SLOT_CLEARCOAT,
    PL_MATERIAL_TEXTURE_SLOT_CLEARCOAT_ROUGHNESS,
    PL_MATERIAL_TEXTURE_SLOT_CLEARCOAT_NORMAL,
    PL_MATERIAL_TEXTURE_SLOT_SHEEN_COLOR,
    PL_MATERIAL_TEXTURE_SLOT_SHEEN_ROUGHNESS,
    PL_MATERIAL_TEXTURE_SLOT_IRIDESCENCE,
    PL_MATERIAL_TEXTURE_SLOT_IRIDESCENCE_THICKNESS,
    PL_MATERIAL_TEXTURE_SLOT_ANISOTROPY,
    PL_MATERIAL_TEXTURE_SLOT_TRANSMISSION,
    PL_MATERIAL_TEXTURE_SLOT_THICKNESS,
    PL_MATERIAL_TEXTURE_SLOT_DIFFUSE_TRANSMISSION,
    PL_MATERIAL_TEXTURE_SLOT_DIFFUSE_TRANSMISSION_COLOR,
    
    PL_MATERIAL_TEXTURE_SLOT_COUNT
};

enum _plMaterialModel
{
    PL_MATERIAL_MODEL_PBR_METALLIC_ROUGHNESS = 0,

    PL_MATERIAL_MODEL_COUNT
};

enum _plMaterialFlags
{
    PL_MATERIAL_FLAG_NONE                = 0,

    PL_MATERIAL_FLAG_CLEARCOAT            = 1 << 0,
    PL_MATERIAL_FLAG_SHEEN                = 1 << 1,
    PL_MATERIAL_FLAG_IRIDESCENCE          = 1 << 2,
    PL_MATERIAL_FLAG_ANISOTROPY           = 1 << 3,
    PL_MATERIAL_FLAG_TRANSMISSION         = 1 << 4,
    PL_MATERIAL_FLAG_VOLUME               = 1 << 5,
    PL_MATERIAL_FLAG_DISPERSION           = 1 << 6,
    PL_MATERIAL_FLAG_DIFFUSE_TRANSMISSION = 1 << 7,

    PL_MATERIAL_FLAG_DOUBLE_SIDED         = 1 << 20,
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plMaterialClearcoat
{
    float fFactor;            // default: 0.0f
    float fRoughness;         // default: 0.0f
    float fNormalMapStrength; // default: 1.0f
} plMaterialClearcoat;

typedef struct _plMaterialSheen
{
    plVec3 tColor;     // default: {0.0f, 0.0f, 0.0f}
    float  fRoughness; // default: 0.0f
} plMaterialSheen;

typedef struct _plMaterialIridescence
{
    float fFactor;        // default: 0.0f
    float fIor;           // default: 1.3f
    float fThicknessMax;  // default: 400.0f
    float fThicknessMin;  // default: 100.0f
} plMaterialIridescence;

typedef struct _plMaterialDispersion
{
    float fDispersion; // default: 0.0f
} plMaterialDispersion;

typedef struct _plMaterialDiffuseTransmission
{
    float  fFactor; // default: 0.0f
    plVec3 tColor;  // default: {1.0f, 1.0f, 1.0f}
} plMaterialDiffuseTransmission;

typedef struct _plMaterialTransmission
{
    float fFactor; // default: 0.0f
} plMaterialTransmission;

typedef struct _plMaterialVolume
{
    float  fThickness;           // default: 0.0f
    float  fAttenuationDistance; // default: 100000.0f
    plVec3 tAttenuationColor;    // default: {1.0f, 1.0f, 1.0f}
} plMaterialVolume;

typedef struct _plMaterialAnisotropy
{
    float fStrength; // default: 0.0f
    float fRotation; // default: 0.0f
} plMaterialAnisotropy;

typedef struct _plMaterialTexture
{
    plAssetHandle tTexture;
    uint32_t      uUVSet;
    plVec2        tOffset;
    plVec2        tScale;
    float         fRotation;
} plMaterialTexture;

typedef struct _plMaterial
{
    plMaterialModel     eMaterialModel; // default: PL_MATERIAL_MODEL_PBR_METALLIC_ROUGHNESS
    plMaterialFlags     eFlags;         // default: PL_MATERIAL_FLAG_NONE
    plMaterialAlphaMode eAlphaMode;     // default: PL_MATERIAL_ALPHA_MODE_BLEND

    // base material
    plVec4 tBaseColor;         // default: {1.0f, 1.0f, 1.0f, 1.0f}
    plVec3 tEmissiveColor;     // default: {0.0f, 0.0f, 0.0f}
    float  fMetalness;         // default: 1.0f
    float  fRoughness;         // default: 1.0f
    float  fNormalMapStrength; // default: 1.0f
    float  fOcclusionStrength; // default: 1.0f
    float  fEmissiveStrength;  // default: 0.0f
    float  fAlphaCutoff;       // default: 0.5f
    float  fIor;               // default: 1.5f
    
    // advanced materials
    plMaterialClearcoat           tClearcoat;
    plMaterialAnisotropy          tAnisotropy;
    plMaterialSheen               tSheen;
    plMaterialIridescence         tIridescence;
    plMaterialDispersion          tDispersion;
    plMaterialTransmission        tTransmission;
    plMaterialDiffuseTransmission tDiffuseTransmission;
    plMaterialVolume              tVolume;

    plMaterialTexture atTextures[PL_MATERIAL_TEXTURE_SLOT_COUNT];
} plMaterial;

#ifdef __cplusplus
}
#endif

#endif // PL_MATERIAL_EXT_H