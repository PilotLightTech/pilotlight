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

#define plMaterialI_version {0, 5, 0}

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
typedef struct _plMaterialWater               plMaterialWater;

// enums & flags
typedef int plMaterialModel;       // -> enum _plMaterialModel       // Enum: (PL_MATERIAL_MODEL_XXXX)
typedef int plMaterialFlags;       // -> enum _plMaterialFlags       // Flag: (PL_MATERIAL_FLAG_XXXX)
typedef int plMaterialTextureSlot; // -> enum _plMaterialTextureSlot // Enum: (PL_MATERIAL_TEXTURE_SLOT_XXXX)
typedef int plMaterialAlphaMode;   // -> enum _plMaterialAlphaMode   // Enum: (PL_MATERIAL_ALPHA_MODE_XXXX)
typedef int plWaterWaveMode;       // -> enum _plWaterWaveMode       // Enum: (PL_WATER_WAVE_MODE_XXXX)

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_material_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_material_ext(plApiRegistryI*, bool reload);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plMaterialI
{
    // basic
    void (*init)(plMaterial*); // initialize all material settings

    // helpers
    plMaterial (*sanitize)(plMaterial); // clamp settings to supported ranges

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
    PL_MATERIAL_MODEL_PBR_METALLIC_ROUGHNESS         = 0,
    PL_MATERIAL_MODEL_PBR_METALLIC_ROUGHNESS_FOLIAGE = 1,
    PL_MATERIAL_MODEL_WATER                          = 2,

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
    PL_MATERIAL_FLAG_EMISSIVE             = 1 << 8,
    PL_MATERIAL_FLAG_FOLIAGE_NORMALS      = 1 << 9,

    PL_MATERIAL_FLAG_DOUBLE_SIDED         = 1 << 20,
};

// Water distances and amplitudes are world units; headings are degrees in XZ.
// Zero amplitude/strength disables that swell/foam layer. Debug: 0 shaded,
// 1 foam coverage, 2 water depth, 3 shaded without shadows,
// 4 refraction only, 5 surface lighting only, 6 composite coverage.
// Wave modes: legacy swells, analytic Gerstner, GPU spectral ocean.
enum _plWaterWaveMode
{
    PL_WATER_WAVE_MODE_SWELL = 0,
    PL_WATER_WAVE_MODE_GERSTNER,
    PL_WATER_WAVE_MODE_FFT
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

typedef struct _plMaterialWater
{
    plWaterWaveMode iWaveMode;            // PL_WATER_WAVE_MODE_*; default swell for older materials
    int             iFftResolution;       // 64, 128 or 256
    int             iFftSeed;             // deterministic spectrum
    float           fGerstnerSteepness;   // gerstner_steepness
    float           fFftPatchSize;        // fft_patch_size
    float           fFftHeight;           // fft_height_limit
    float           fFftWindSpeed;        // fft_wind_speed
    float           fFftWindHeading;      // fft_wind_heading_degrees
    float           fFftWindDependency;   // fft_against_wind
    float           fFftDamping;          // fft_short_wave_damping
    float           fFftChoppiness;       // fft_choppiness
    float           fFftSpeed;            // fft_speed
    float           fAmplitude0;          // swell0_amplitude, default 20
    float           fWavelength0;         // swell0_wavelength, default 2048
    float           fSpeed0;              // swell0_speed, default 5
    float           fHeading0;            // swell0_heading_degrees, default 180
    float           fAmplitude1;          // swell1_amplitude, default 3
    float           fWavelength1;         // swell1_wavelength, default 1024
    float           fSpeed1;              // swell1_speed, default 1
    float           fHeading1;            // swell1_heading_degrees, default 180
    float           fDetailScale;         // detail_scale, default 1
    float           fDetailSpeed;         // detail_speed, default 1
    float           fFoamSpeed;           // foam_speed, default 1
    float           fFoamScale;           // foam_scale, default 1
    float           fShoreStrength;       // shore_strength, default 1
    float           fShoreDepth;          // shore_depth, default 12
    float           fShoreRayStart;       // shore_ray_fade_start, default 24
    float           fShoreRayEnd;         // shore_ray_fade_end, default 48
    float           fWhitecapStrength;    // whitecap_strength, default 0.75
    float           fCrestStart;          // crest_start, default 0.9
    float           fCrestFull;           // crest_full, default 1
    float           fSlopeStart;          // slope_start, default 0.008
    float           fSlopeFull;           // slope_full, default 0.035
    float           fMinRoughness;        // minimum_roughness, default 0.08
    float           fFoamRoughness;       // foam_roughness, default 0.75
    float           fFoamNormalFlatten;   // foam_normal_flatten, default 0.65
    float           fDistortionStrength;  // refraction_strength, default 1
    float           fMaxDistortionPixels; // refraction_max_pixels, default 12
    float           fAbsorptionRed;       // absorption_red, default 0.55
    float           fAbsorptionGreen;     // absorption_green, default 0.85
    float           fAbsorptionBlue;      // absorption_blue, default 0.92
    float           fAbsorptionDistance;  // absorption_distance, default 128.0
    float           fScatteringRed;       // scattering_red, default 0.2
    float           fScatteringGreen;     // scattering_green, default 0.5
    float           fScatteringBlue;      // scattering_blue, default 0.6
    float           fScatteringStrength;  // scattering_strength, default 0.005
    float           fScatteringDirection; // scattering_direction, default 0.35
    float           fMaxPath;             // refraction_max_distance, default 128
    float           fOpacityDistance;     // opacity_distance, default 128
    float           fEdgeFade;            // refraction_edge_fade_pixels, default 24
    float           fIntersectionFade;    // refraction_intersection_fade, default 2
    float           fFoamRed;             // foam_red, default 0.85
    float           fFoamGreen;           // foam_green, default 0.9
    float           fFoamBlue;            // foam_blue, default 0.88
    float           fDebugView;           // debug_view, default 0
} plMaterialWater;

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
    float  fEmissiveStrength;  // default: 1.0f
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
    plMaterialWater               tWater;

    plMaterialTexture atTextures[PL_MATERIAL_TEXTURE_SLOT_COUNT];
} plMaterial;

#ifdef __cplusplus
}
#endif

#endif // PL_MATERIAL_EXT_H
