/*
   pl_material_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] global data
// [SECTION] public api implementations
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <string.h> // memset
#include <math.h> // legacy absorption migration
#include "pl.h"
#include "pl_material_ext.h"

// extensions
#include "pl_asset_ext.h"
#include "pl_vfs_ext.h"
#include "pl_json_ext.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plVfsI*    gptVfs    = NULL;
    static const plAssetI*  gptAsset  = NULL;
    static const plMemoryI* gptMemory = NULL;
    static const plJsonI* gptJson = NULL;

    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
#endif



typedef struct _plMaterialContext
{
    plAssetTypeKey tAssetTypeKey;

} plMaterialContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static const char* gapcTextureSlotNames[] = {
    "base_color",
    "normal",
    "emissive",
    "occlusion",
    "metal_roughness",
    "clearcoat",
    "clearcoat_roughness",
    "clearcoat_normal",
    "sheen_color",
    "sheen_roughness",
    "iridescence",
    "iridescence_thickness",
    "anisotropy",
    "transmission_color",
    "transmission_thickness",
    "diffuse_transmission",
    "diffuse_transmission_color"
};

static plMaterialContext* gptMaterialCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementations
//-----------------------------------------------------------------------------

static plMaterial
pl_material_sanitize(plMaterial tMaterial)
{
    tMaterial.tWater.fAmplitude0 = tMaterial.tWater.fAmplitude0 != tMaterial.tWater.fAmplitude0 ? 20.0f : (tMaterial.tWater.fAmplitude0 < 0.0f ? 0.0f : (tMaterial.tWater.fAmplitude0 > 10000.0f ? 10000.0f : tMaterial.tWater.fAmplitude0));
    tMaterial.tWater.fWavelength0 = tMaterial.tWater.fWavelength0 != tMaterial.tWater.fWavelength0 ? 2048.0f : (tMaterial.tWater.fWavelength0 < 0.01f ? 0.01f : (tMaterial.tWater.fWavelength0 > 1000000.0f ? 1000000.0f : tMaterial.tWater.fWavelength0));
    tMaterial.tWater.fSpeed0 = tMaterial.tWater.fSpeed0 != tMaterial.tWater.fSpeed0 ? 5.0f : (tMaterial.tWater.fSpeed0 < -100.0f ? -100.0f : (tMaterial.tWater.fSpeed0 > 100.0f ? 100.0f : tMaterial.tWater.fSpeed0));
    tMaterial.tWater.fHeading0 = tMaterial.tWater.fHeading0 != tMaterial.tWater.fHeading0 ? 180.0f : (tMaterial.tWater.fHeading0 < -360.0f ? -360.0f : (tMaterial.tWater.fHeading0 > 360.0f ? 360.0f : tMaterial.tWater.fHeading0));
    tMaterial.tWater.fAmplitude1 = tMaterial.tWater.fAmplitude1 != tMaterial.tWater.fAmplitude1 ? 3.0f : (tMaterial.tWater.fAmplitude1 < 0.0f ? 0.0f : (tMaterial.tWater.fAmplitude1 > 10000.0f ? 10000.0f : tMaterial.tWater.fAmplitude1));
    tMaterial.tWater.fWavelength1 = tMaterial.tWater.fWavelength1 != tMaterial.tWater.fWavelength1 ? 1024.0f : (tMaterial.tWater.fWavelength1 < 0.01f ? 0.01f : (tMaterial.tWater.fWavelength1 > 1000000.0f ? 1000000.0f : tMaterial.tWater.fWavelength1));
    tMaterial.tWater.fSpeed1 = tMaterial.tWater.fSpeed1 != tMaterial.tWater.fSpeed1 ? 1.0f : (tMaterial.tWater.fSpeed1 < -100.0f ? -100.0f : (tMaterial.tWater.fSpeed1 > 100.0f ? 100.0f : tMaterial.tWater.fSpeed1));
    tMaterial.tWater.fHeading1 = tMaterial.tWater.fHeading1 != tMaterial.tWater.fHeading1 ? 180.0f : (tMaterial.tWater.fHeading1 < -360.0f ? -360.0f : (tMaterial.tWater.fHeading1 > 360.0f ? 360.0f : tMaterial.tWater.fHeading1));
    tMaterial.tWater.fDetailScale = tMaterial.tWater.fDetailScale != tMaterial.tWater.fDetailScale ? 1.0f : (tMaterial.tWater.fDetailScale < 0.001f ? 0.001f : (tMaterial.tWater.fDetailScale > 10000.0f ? 10000.0f : tMaterial.tWater.fDetailScale));
    tMaterial.tWater.fDetailSpeed = tMaterial.tWater.fDetailSpeed != tMaterial.tWater.fDetailSpeed ? 1.0f : (tMaterial.tWater.fDetailSpeed < -100.0f ? -100.0f : (tMaterial.tWater.fDetailSpeed > 100.0f ? 100.0f : tMaterial.tWater.fDetailSpeed));
    tMaterial.tWater.fFoamSpeed = tMaterial.tWater.fFoamSpeed != tMaterial.tWater.fFoamSpeed ? 1.0f : (tMaterial.tWater.fFoamSpeed < -100.0f ? -100.0f : (tMaterial.tWater.fFoamSpeed > 100.0f ? 100.0f : tMaterial.tWater.fFoamSpeed));
    tMaterial.tWater.fFoamScale = tMaterial.tWater.fFoamScale != tMaterial.tWater.fFoamScale ? 1.0f : (tMaterial.tWater.fFoamScale < 0.001f ? 0.001f : (tMaterial.tWater.fFoamScale > 10000.0f ? 10000.0f : tMaterial.tWater.fFoamScale));
    tMaterial.tWater.fShoreStrength = tMaterial.tWater.fShoreStrength != tMaterial.tWater.fShoreStrength ? 1.0f : (tMaterial.tWater.fShoreStrength < 0.0f ? 0.0f : (tMaterial.tWater.fShoreStrength > 1.0f ? 1.0f : tMaterial.tWater.fShoreStrength));
    tMaterial.tWater.fShoreDepth = tMaterial.tWater.fShoreDepth != tMaterial.tWater.fShoreDepth ? 12.0f : (tMaterial.tWater.fShoreDepth < 0.001f ? 0.001f : (tMaterial.tWater.fShoreDepth > 100000.0f ? 100000.0f : tMaterial.tWater.fShoreDepth));
    tMaterial.tWater.fShoreRayStart = tMaterial.tWater.fShoreRayStart != tMaterial.tWater.fShoreRayStart ? 24.0f : (tMaterial.tWater.fShoreRayStart < 0.0f ? 0.0f : (tMaterial.tWater.fShoreRayStart > 100000.0f ? 100000.0f : tMaterial.tWater.fShoreRayStart));
    tMaterial.tWater.fShoreRayEnd = tMaterial.tWater.fShoreRayEnd != tMaterial.tWater.fShoreRayEnd ? 48.0f : (tMaterial.tWater.fShoreRayEnd < 0.001f ? 0.001f : (tMaterial.tWater.fShoreRayEnd > 100000.0f ? 100000.0f : tMaterial.tWater.fShoreRayEnd));
    tMaterial.tWater.fWhitecapStrength = tMaterial.tWater.fWhitecapStrength != tMaterial.tWater.fWhitecapStrength ? 0.75f : (tMaterial.tWater.fWhitecapStrength < 0.0f ? 0.0f : (tMaterial.tWater.fWhitecapStrength > 1.0f ? 1.0f : tMaterial.tWater.fWhitecapStrength));
    tMaterial.tWater.fCrestStart = tMaterial.tWater.fCrestStart != tMaterial.tWater.fCrestStart ? 0.9f : (tMaterial.tWater.fCrestStart < -1.0f ? -1.0f : (tMaterial.tWater.fCrestStart > 1.0f ? 1.0f : tMaterial.tWater.fCrestStart));
    tMaterial.tWater.fCrestFull = tMaterial.tWater.fCrestFull != tMaterial.tWater.fCrestFull ? 1.0f : (tMaterial.tWater.fCrestFull < -1.0f ? -1.0f : (tMaterial.tWater.fCrestFull > 1.0f ? 1.0f : tMaterial.tWater.fCrestFull));
    tMaterial.tWater.fSlopeStart = tMaterial.tWater.fSlopeStart != tMaterial.tWater.fSlopeStart ? 0.008f : (tMaterial.tWater.fSlopeStart < 0.0f ? 0.0f : (tMaterial.tWater.fSlopeStart > 10.0f ? 10.0f : tMaterial.tWater.fSlopeStart));
    tMaterial.tWater.fSlopeFull = tMaterial.tWater.fSlopeFull != tMaterial.tWater.fSlopeFull ? 0.035f : (tMaterial.tWater.fSlopeFull < 0.0f ? 0.0f : (tMaterial.tWater.fSlopeFull > 10.0f ? 10.0f : tMaterial.tWater.fSlopeFull));
    tMaterial.tWater.fMinRoughness = tMaterial.tWater.fMinRoughness != tMaterial.tWater.fMinRoughness ? 0.08f : (tMaterial.tWater.fMinRoughness < 0.0f ? 0.0f : (tMaterial.tWater.fMinRoughness > 1.0f ? 1.0f : tMaterial.tWater.fMinRoughness));
    tMaterial.tWater.fFoamRoughness = tMaterial.tWater.fFoamRoughness != tMaterial.tWater.fFoamRoughness ? 0.75f : (tMaterial.tWater.fFoamRoughness < 0.0f ? 0.0f : (tMaterial.tWater.fFoamRoughness > 1.0f ? 1.0f : tMaterial.tWater.fFoamRoughness));
    tMaterial.tWater.fFoamNormalFlatten = tMaterial.tWater.fFoamNormalFlatten != tMaterial.tWater.fFoamNormalFlatten ? 0.65f : (tMaterial.tWater.fFoamNormalFlatten < 0.0f ? 0.0f : (tMaterial.tWater.fFoamNormalFlatten > 1.0f ? 1.0f : tMaterial.tWater.fFoamNormalFlatten));
    tMaterial.tWater.fDistortionStrength = tMaterial.tWater.fDistortionStrength != tMaterial.tWater.fDistortionStrength ? 1.0f : (tMaterial.tWater.fDistortionStrength < 0.0f ? 0.0f : (tMaterial.tWater.fDistortionStrength > 8.0f ? 8.0f : tMaterial.tWater.fDistortionStrength));
    tMaterial.tWater.fMaxDistortionPixels = tMaterial.tWater.fMaxDistortionPixels != tMaterial.tWater.fMaxDistortionPixels ? 12.0f : (tMaterial.tWater.fMaxDistortionPixels < 0.0f ? 0.0f : (tMaterial.tWater.fMaxDistortionPixels > 64.0f ? 64.0f : tMaterial.tWater.fMaxDistortionPixels));
    tMaterial.tWater.fAbsorptionRed = tMaterial.tWater.fAbsorptionRed != tMaterial.tWater.fAbsorptionRed ? 0.55f : (tMaterial.tWater.fAbsorptionRed < 0.0001f ? 0.0001f : (tMaterial.tWater.fAbsorptionRed > 1.0f ? 1.0f : tMaterial.tWater.fAbsorptionRed));
    tMaterial.tWater.fAbsorptionGreen = tMaterial.tWater.fAbsorptionGreen != tMaterial.tWater.fAbsorptionGreen ? 0.85f : (tMaterial.tWater.fAbsorptionGreen < 0.0001f ? 0.0001f : (tMaterial.tWater.fAbsorptionGreen > 1.0f ? 1.0f : tMaterial.tWater.fAbsorptionGreen));
    tMaterial.tWater.fAbsorptionBlue = tMaterial.tWater.fAbsorptionBlue != tMaterial.tWater.fAbsorptionBlue ? 0.92f : (tMaterial.tWater.fAbsorptionBlue < 0.0001f ? 0.0001f : (tMaterial.tWater.fAbsorptionBlue > 1.0f ? 1.0f : tMaterial.tWater.fAbsorptionBlue));
    tMaterial.tWater.fAbsorptionDistance = tMaterial.tWater.fAbsorptionDistance != tMaterial.tWater.fAbsorptionDistance ? 128.0f : (tMaterial.tWater.fAbsorptionDistance < 0.001f ? 0.001f : (tMaterial.tWater.fAbsorptionDistance > 100000.0f ? 100000.0f : tMaterial.tWater.fAbsorptionDistance));
    tMaterial.tWater.fScatteringRed = tMaterial.tWater.fScatteringRed != tMaterial.tWater.fScatteringRed ? 0.2f : (tMaterial.tWater.fScatteringRed < 0.0f ? 0.0f : (tMaterial.tWater.fScatteringRed > 1.0f ? 1.0f : tMaterial.tWater.fScatteringRed));
    tMaterial.tWater.fScatteringGreen = tMaterial.tWater.fScatteringGreen != tMaterial.tWater.fScatteringGreen ? 0.5f : (tMaterial.tWater.fScatteringGreen < 0.0f ? 0.0f : (tMaterial.tWater.fScatteringGreen > 1.0f ? 1.0f : tMaterial.tWater.fScatteringGreen));
    tMaterial.tWater.fScatteringBlue = tMaterial.tWater.fScatteringBlue != tMaterial.tWater.fScatteringBlue ? 0.6f : (tMaterial.tWater.fScatteringBlue < 0.0f ? 0.0f : (tMaterial.tWater.fScatteringBlue > 1.0f ? 1.0f : tMaterial.tWater.fScatteringBlue));
    tMaterial.tWater.fScatteringStrength = tMaterial.tWater.fScatteringStrength != tMaterial.tWater.fScatteringStrength ? 0.005f : (tMaterial.tWater.fScatteringStrength < 0.0f ? 0.0f : (tMaterial.tWater.fScatteringStrength > 1000.0f ? 1000.0f : tMaterial.tWater.fScatteringStrength));
    tMaterial.tWater.fScatteringDirection = tMaterial.tWater.fScatteringDirection != tMaterial.tWater.fScatteringDirection ? 0.35f : (tMaterial.tWater.fScatteringDirection < -0.9f ? -0.9f : (tMaterial.tWater.fScatteringDirection > 0.9f ? 0.9f : tMaterial.tWater.fScatteringDirection));
    tMaterial.tWater.fMaxPath = tMaterial.tWater.fMaxPath != tMaterial.tWater.fMaxPath ? 128.0f : (tMaterial.tWater.fMaxPath < 0.001f ? 0.001f : (tMaterial.tWater.fMaxPath > 100000.0f ? 100000.0f : tMaterial.tWater.fMaxPath));
    tMaterial.tWater.fOpacityDistance = tMaterial.tWater.fOpacityDistance != tMaterial.tWater.fOpacityDistance ? 128.0f : (tMaterial.tWater.fOpacityDistance < 0.001f ? 0.001f : (tMaterial.tWater.fOpacityDistance > 100000.0f ? 100000.0f : tMaterial.tWater.fOpacityDistance));
    tMaterial.tWater.fEdgeFade = tMaterial.tWater.fEdgeFade != tMaterial.tWater.fEdgeFade ? 24.0f : (tMaterial.tWater.fEdgeFade < 0.001f ? 0.001f : (tMaterial.tWater.fEdgeFade > 1000.0f ? 1000.0f : tMaterial.tWater.fEdgeFade));
    tMaterial.tWater.fIntersectionFade = tMaterial.tWater.fIntersectionFade != tMaterial.tWater.fIntersectionFade ? 2.0f : (tMaterial.tWater.fIntersectionFade < 0.001f ? 0.001f : (tMaterial.tWater.fIntersectionFade > 100000.0f ? 100000.0f : tMaterial.tWater.fIntersectionFade));
    tMaterial.tWater.fFoamRed = tMaterial.tWater.fFoamRed != tMaterial.tWater.fFoamRed ? 0.85f : (tMaterial.tWater.fFoamRed < 0.0f ? 0.0f : (tMaterial.tWater.fFoamRed > 1.0f ? 1.0f : tMaterial.tWater.fFoamRed));
    tMaterial.tWater.fFoamGreen = tMaterial.tWater.fFoamGreen != tMaterial.tWater.fFoamGreen ? 0.9f : (tMaterial.tWater.fFoamGreen < 0.0f ? 0.0f : (tMaterial.tWater.fFoamGreen > 1.0f ? 1.0f : tMaterial.tWater.fFoamGreen));
    tMaterial.tWater.fFoamBlue = tMaterial.tWater.fFoamBlue != tMaterial.tWater.fFoamBlue ? 0.88f : (tMaterial.tWater.fFoamBlue < 0.0f ? 0.0f : (tMaterial.tWater.fFoamBlue > 1.0f ? 1.0f : tMaterial.tWater.fFoamBlue));
    tMaterial.tWater.fDebugView = tMaterial.tWater.fDebugView != tMaterial.tWater.fDebugView ? 0.0f : (tMaterial.tWater.fDebugView < 0.0f ? 0.0f : (tMaterial.tWater.fDebugView > 8.0f ? 8.0f : tMaterial.tWater.fDebugView));
    // Keep fade intervals ordered and wide enough for float precision.
    if(tMaterial.tWater.fCrestFull < -0.9999f)
        tMaterial.tWater.fCrestFull = -0.9999f;
    if(tMaterial.tWater.fSlopeFull < 0.0001f)
        tMaterial.tWater.fSlopeFull = 0.0001f;
    if(tMaterial.tWater.fShoreRayStart > tMaterial.tWater.fShoreRayEnd - 0.001f * tMaterial.tWater.fShoreRayEnd)
        tMaterial.tWater.fShoreRayStart = tMaterial.tWater.fShoreRayEnd - 0.001f * tMaterial.tWater.fShoreRayEnd;
    if(tMaterial.tWater.fCrestStart > tMaterial.tWater.fCrestFull - 0.0001f)
        tMaterial.tWater.fCrestStart = tMaterial.tWater.fCrestFull - 0.0001f;
    if(tMaterial.tWater.fSlopeStart > tMaterial.tWater.fSlopeFull - 0.0001f)
        tMaterial.tWater.fSlopeStart = tMaterial.tWater.fSlopeFull - 0.0001f;
    tMaterial.tWater.fDebugView = (float)(int)tMaterial.tWater.fDebugView;
    tMaterial.tWater.iWaveMode = tMaterial.tWater.iWaveMode < 0 || tMaterial.tWater.iWaveMode > 2 ? 0 : tMaterial.tWater.iWaveMode;
    tMaterial.tWater.iFftResolution = tMaterial.tWater.iFftResolution <= 64 ? 64 : (tMaterial.tWater.iFftResolution <= 128 ? 128 : 256);
    tMaterial.tWater.fGerstnerSteepness = tMaterial.tWater.fGerstnerSteepness != tMaterial.tWater.fGerstnerSteepness ? 0.65f : (tMaterial.tWater.fGerstnerSteepness < 0.0f ? 0.0f : (tMaterial.tWater.fGerstnerSteepness > 0.95f ? 0.95f : tMaterial.tWater.fGerstnerSteepness));
    tMaterial.tWater.fFftPatchSize = tMaterial.tWater.fFftPatchSize != tMaterial.tWater.fFftPatchSize ? 4096.0f : (tMaterial.tWater.fFftPatchSize < 0.1f ? 0.1f : (tMaterial.tWater.fFftPatchSize > 1000000.0f ? 1000000.0f : tMaterial.tWater.fFftPatchSize));
    tMaterial.tWater.fFftHeight = tMaterial.tWater.fFftHeight != tMaterial.tWater.fFftHeight ? 30.0f : (tMaterial.tWater.fFftHeight < 0.0f ? 0.0f : (tMaterial.tWater.fFftHeight > 10000.0f ? 10000.0f : tMaterial.tWater.fFftHeight));
    tMaterial.tWater.fFftWindSpeed = tMaterial.tWater.fFftWindSpeed != tMaterial.tWater.fFftWindSpeed ? 30.0f : (tMaterial.tWater.fFftWindSpeed < 0.0f ? 0.0f : (tMaterial.tWater.fFftWindSpeed > 1000.0f ? 1000.0f : tMaterial.tWater.fFftWindSpeed));
    tMaterial.tWater.fFftWindHeading = tMaterial.tWater.fFftWindHeading != tMaterial.tWater.fFftWindHeading ? 180.0f : (tMaterial.tWater.fFftWindHeading < -360.0f ? -360.0f : (tMaterial.tWater.fFftWindHeading > 360.0f ? 360.0f : tMaterial.tWater.fFftWindHeading));
    tMaterial.tWater.fFftWindDependency = tMaterial.tWater.fFftWindDependency != tMaterial.tWater.fFftWindDependency ? 0.07f : (tMaterial.tWater.fFftWindDependency < 0.0f ? 0.0f : (tMaterial.tWater.fFftWindDependency > 1.0f ? 1.0f : tMaterial.tWater.fFftWindDependency));
    tMaterial.tWater.fFftDamping = tMaterial.tWater.fFftDamping != tMaterial.tWater.fFftDamping ? 1.0f : (tMaterial.tWater.fFftDamping < 0.0f ? 0.0f : (tMaterial.tWater.fFftDamping > 10000.0f ? 10000.0f : tMaterial.tWater.fFftDamping));
    tMaterial.tWater.fFftChoppiness = tMaterial.tWater.fFftChoppiness != tMaterial.tWater.fFftChoppiness ? 1.0f : (tMaterial.tWater.fFftChoppiness < 0.0f ? 0.0f : (tMaterial.tWater.fFftChoppiness > 3.0f ? 3.0f : tMaterial.tWater.fFftChoppiness));
    tMaterial.tWater.fFftSpeed = tMaterial.tWater.fFftSpeed != tMaterial.tWater.fFftSpeed ? 1.0f : (tMaterial.tWater.fFftSpeed < -100.0f ? -100.0f : (tMaterial.tWater.fFftSpeed > 100.0f ? 100.0f : tMaterial.tWater.fFftSpeed));
    return tMaterial;
}

static const plMaterialWater gtMaterialWaterDefault = {
    .iFftResolution = 128,
    .iFftSeed = 1,
    .fGerstnerSteepness = 0.65f,
    .fFftPatchSize = 4096.0f,
    .fFftHeight = 30.0f,
    .fFftWindSpeed = 30.0f,
    .fFftWindHeading = 180.0f,
    .fFftWindDependency = 0.07f,
    .fFftDamping = 1.0f,
    .fFftChoppiness = 1.0f,
    .fFftSpeed = 1.0f,
    .fAmplitude0 = 20.0f,
    .fWavelength0 = 2048.0f,
    .fSpeed0 = 5.0f,
    .fHeading0 = 180.0f,
    .fAmplitude1 = 3.0f,
    .fWavelength1 = 1024.0f,
    .fSpeed1 = 1.0f,
    .fHeading1 = 180.0f,
    .fDetailScale = 1.0f,
    .fDetailSpeed = 1.0f,
    .fFoamSpeed = 1.0f,
    .fFoamScale = 1.0f,
    .fShoreStrength = 1.0f,
    .fShoreDepth = 12.0f,
    .fShoreRayStart = 24.0f,
    .fShoreRayEnd = 48.0f,
    .fWhitecapStrength = 0.75f,
    .fCrestStart = 0.9f,
    .fCrestFull = 1.0f,
    .fSlopeStart = 0.008f,
    .fSlopeFull = 0.035f,
    .fMinRoughness = 0.08f,
    .fFoamRoughness = 0.75f,
    .fFoamNormalFlatten = 0.65f,
    .fDistortionStrength = 1.0f,
    .fMaxDistortionPixels = 12.0f,
    .fAbsorptionRed = 0.55f,
    .fAbsorptionGreen = 0.85f,
    .fAbsorptionBlue = 0.92f,
    .fAbsorptionDistance = 128.0f,
    .fScatteringRed = 0.2f,
    .fScatteringGreen = 0.5f,
    .fScatteringBlue = 0.6f,
    .fScatteringStrength = 0.005f,
    .fScatteringDirection = 0.35f,
    .fMaxPath = 128.0f,
    .fOpacityDistance = 128.0f,
    .fEdgeFade = 24.0f,
    .fIntersectionFade = 2.0f,
    .fFoamRed = 0.85f,
    .fFoamGreen = 0.9f,
    .fFoamBlue = 0.88f,
    .fDebugView = 0.0f,
};

void
pl_material_init(plMaterial* ptMaterial)
{
    const plMaterial tMaterialDefault = {
        .eAlphaMode            = PL_MATERIAL_ALPHA_MODE_OPAQUE,
        .eFlags                = PL_MATERIAL_FLAG_NONE,
        .eMaterialModel        = PL_MATERIAL_MODEL_PBR_METALLIC_ROUGHNESS,
        .tBaseColor            = {1.0f, 1.0f, 1.0f, 1.0f},
        .tEmissiveColor        = {0.0f, 0.0f, 0.0f},
        .fRoughness            = 1.0f,
        .fMetalness            = 1.0f,
        .fAlphaCutoff          = 0.5f,
        .fNormalMapStrength    = 1.0f,
        .fEmissiveStrength     = 1.0f,
        .fOcclusionStrength    = 1.0f,
        .fIor                  = 1.5f,
        .tVolume = {
            .fThickness = 0.0f,
            .tAttenuationColor = {1.0f, 1.0f, 1.0f},
            .fAttenuationDistance = 100000.0f
        },
        .tDiffuseTransmission = {
            .fFactor = 0.0f,
            .tColor = {1.0f, 1.0f, 1.0f}
        },
        .tSheen = {
            .fRoughness = 0.0f,
            .tColor = {0}
        },
        .tIridescence = {
            .fIor = 1.3f,
            .fThicknessMin = 100.0f,
            .fThicknessMax = 400.0f
        },
        .tClearcoat = {
            .fFactor = 0.0f,
            .fRoughness = 0.0f,
            .fNormalMapStrength = 1.0f
        },
        .atTextures = {0}
    };
    *ptMaterial = tMaterialDefault;
    ptMaterial->tWater = gtMaterialWaterDefault;
}

static bool
pl__material_serialize(const char* pcName, const void* pMaterial, plAssetEncoding eEncoding)
{
    if(eEncoding == PL_ASSET_ENCODING_BINARY)
        return false;
        
    const plMaterial* ptMaterial = pMaterial;
    plJsonObject* ptRoot = gptJson->new_root_object("root");
    gptJson->add_string_member(ptRoot, "format", "plmaterial");
    gptJson->add_uint32_member(ptRoot, "version", 1);

    switch(ptMaterial->eMaterialModel)
    {
        case PL_MATERIAL_MODEL_PBR_METALLIC_ROUGHNESS:
            gptJson->add_string_member(ptRoot, "model", "pbr_metallic_roughness");
            break;

        case PL_MATERIAL_MODEL_PBR_METALLIC_ROUGHNESS_FOLIAGE:
            gptJson->add_string_member(ptRoot, "model", "pbr_metallic_roughness_foliage");
            break;

        case PL_MATERIAL_MODEL_WATER:
            gptJson->add_string_member(ptRoot, "model", "water");
            break;

        default:
            PL_ASSERT(false && "unknown material model");
            break;

    }

    if(ptMaterial->eMaterialModel == PL_MATERIAL_MODEL_WATER)
    {
        plJsonObject* ptWater = gptJson->add_member(ptRoot, "water");
        const plMaterialWater tWater = ptMaterial->tWater;
        const char* apcWaveModes[] = {"swell", "gerstner", "fft"};
        gptJson->add_string_member(ptWater, "wave_mode", apcWaveModes[tWater.iWaveMode]);
        gptJson->add_int_member(ptWater, "fft_resolution", tWater.iFftResolution);
        gptJson->add_int_member(ptWater, "fft_seed", tWater.iFftSeed);
        gptJson->add_float_member(ptWater, "gerstner_steepness", tWater.fGerstnerSteepness);
        gptJson->add_float_member(ptWater, "fft_patch_size", tWater.fFftPatchSize);
        gptJson->add_float_member(ptWater, "fft_height_limit", tWater.fFftHeight);
        gptJson->add_float_member(ptWater, "fft_wind_speed", tWater.fFftWindSpeed);
        gptJson->add_float_member(ptWater, "fft_wind_heading_degrees", tWater.fFftWindHeading);
        gptJson->add_float_member(ptWater, "fft_against_wind", tWater.fFftWindDependency);
        gptJson->add_float_member(ptWater, "fft_short_wave_damping", tWater.fFftDamping);
        gptJson->add_float_member(ptWater, "fft_choppiness", tWater.fFftChoppiness);
        gptJson->add_float_member(ptWater, "fft_speed", tWater.fFftSpeed);
        gptJson->add_float_member(ptWater, "swell0_amplitude", tWater.fAmplitude0);
        gptJson->add_float_member(ptWater, "swell0_wavelength", tWater.fWavelength0);
        gptJson->add_float_member(ptWater, "swell0_speed", tWater.fSpeed0);
        gptJson->add_float_member(ptWater, "swell0_heading_degrees", tWater.fHeading0);
        gptJson->add_float_member(ptWater, "swell1_amplitude", tWater.fAmplitude1);
        gptJson->add_float_member(ptWater, "swell1_wavelength", tWater.fWavelength1);
        gptJson->add_float_member(ptWater, "swell1_speed", tWater.fSpeed1);
        gptJson->add_float_member(ptWater, "swell1_heading_degrees", tWater.fHeading1);
        gptJson->add_float_member(ptWater, "detail_scale", tWater.fDetailScale);
        gptJson->add_float_member(ptWater, "detail_speed", tWater.fDetailSpeed);
        gptJson->add_float_member(ptWater, "foam_speed", tWater.fFoamSpeed);
        gptJson->add_float_member(ptWater, "foam_scale", tWater.fFoamScale);
        gptJson->add_float_member(ptWater, "shore_strength", tWater.fShoreStrength);
        gptJson->add_float_member(ptWater, "shore_depth", tWater.fShoreDepth);
        gptJson->add_float_member(ptWater, "shore_ray_fade_start", tWater.fShoreRayStart);
        gptJson->add_float_member(ptWater, "shore_ray_fade_end", tWater.fShoreRayEnd);
        gptJson->add_float_member(ptWater, "whitecap_strength", tWater.fWhitecapStrength);
        gptJson->add_float_member(ptWater, "crest_start", tWater.fCrestStart);
        gptJson->add_float_member(ptWater, "crest_full", tWater.fCrestFull);
        gptJson->add_float_member(ptWater, "slope_start", tWater.fSlopeStart);
        gptJson->add_float_member(ptWater, "slope_full", tWater.fSlopeFull);
        gptJson->add_float_member(ptWater, "minimum_roughness", tWater.fMinRoughness);
        gptJson->add_float_member(ptWater, "foam_roughness", tWater.fFoamRoughness);
        gptJson->add_float_member(ptWater, "foam_normal_flatten", tWater.fFoamNormalFlatten);
        gptJson->add_float_member(ptWater, "refraction_strength", tWater.fDistortionStrength);
        gptJson->add_float_member(ptWater, "refraction_max_pixels", tWater.fMaxDistortionPixels);
        gptJson->add_float_member(ptWater, "absorption_red", tWater.fAbsorptionRed);
        gptJson->add_float_member(ptWater, "absorption_green", tWater.fAbsorptionGreen);
        gptJson->add_float_member(ptWater, "absorption_blue", tWater.fAbsorptionBlue);
        gptJson->add_float_member(ptWater, "absorption_distance", tWater.fAbsorptionDistance);
        gptJson->add_float_member(ptWater, "scattering_red", tWater.fScatteringRed);
        gptJson->add_float_member(ptWater, "scattering_green", tWater.fScatteringGreen);
        gptJson->add_float_member(ptWater, "scattering_blue", tWater.fScatteringBlue);
        gptJson->add_float_member(ptWater, "scattering_strength", tWater.fScatteringStrength);
        gptJson->add_float_member(ptWater, "scattering_direction", tWater.fScatteringDirection);
        gptJson->add_float_member(ptWater, "refraction_max_distance", tWater.fMaxPath);
        gptJson->add_float_member(ptWater, "opacity_distance", tWater.fOpacityDistance);
        gptJson->add_float_member(ptWater, "refraction_edge_fade_pixels", tWater.fEdgeFade);
        gptJson->add_float_member(ptWater, "refraction_intersection_fade", tWater.fIntersectionFade);
        gptJson->add_float_member(ptWater, "foam_red", tWater.fFoamRed);
        gptJson->add_float_member(ptWater, "foam_green", tWater.fFoamGreen);
        gptJson->add_float_member(ptWater, "foam_blue", tWater.fFoamBlue);
        gptJson->add_float_member(ptWater, "debug_view", tWater.fDebugView);
    }

    // base material stuff
    gptJson->add_float_array(ptRoot, "base_color", ptMaterial->tBaseColor.d, 4);
    gptJson->add_float_member(ptRoot, "metalness", ptMaterial->fMetalness);
    gptJson->add_float_member(ptRoot, "roughness", ptMaterial->fRoughness);
    gptJson->add_float_member(ptRoot, "ior", ptMaterial->fIor);
    gptJson->add_float_member(ptRoot, "normal_map_strength", ptMaterial->fNormalMapStrength);
    gptJson->add_float_member(ptRoot, "occlusion_strength", ptMaterial->fOcclusionStrength);
    gptJson->add_bool_member(ptRoot, "double_sided", (bool)(ptMaterial->eFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED));
    gptJson->add_bool_member(ptRoot, "foliage_normals", (bool)(ptMaterial->eFlags & PL_MATERIAL_FLAG_FOLIAGE_NORMALS));

    // alpha stuff
    plJsonObject* ptAlpha = gptJson->add_member(ptRoot, "alpha");
    switch(ptMaterial->eAlphaMode)
    {
        case PL_MATERIAL_ALPHA_MODE_MASK:
            gptJson->add_string_member(ptAlpha, "mode", "mask");
            break;

        case PL_MATERIAL_ALPHA_MODE_BLEND:
            gptJson->add_string_member(ptAlpha, "mode", "blend");
            break;
        
        case PL_MATERIAL_ALPHA_MODE_OPAQUE:
        default:
            gptJson->add_string_member(ptAlpha, "mode", "opaque");
            break;
    }
    gptJson->add_float_member(ptAlpha, "cutoff", ptMaterial->fAlphaCutoff);

    // clearcoat stuff
    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_CLEARCOAT)
    {
        plJsonObject* ptAdvanced = gptJson->add_member(ptRoot, "clearcoat");
        gptJson->add_float_member(ptAdvanced, "factor", ptMaterial->tClearcoat.fFactor);
        gptJson->add_float_member(ptAdvanced, "roughness", ptMaterial->tClearcoat.fRoughness);
        gptJson->add_float_member(ptAdvanced, "normal_map_strength", ptMaterial->tClearcoat.fNormalMapStrength);
    }

    // sheen stuff
    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_SHEEN)
    {
        plJsonObject* ptAdvanced = gptJson->add_member(ptRoot, "sheen");
        gptJson->add_float_array(ptAdvanced, "color", ptMaterial->tSheen.tColor.d, 3);
        gptJson->add_float_member(ptAdvanced, "roughness", ptMaterial->tSheen.fRoughness);
    }

    // iridescence stuff
    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_IRIDESCENCE)
    {
        plJsonObject* ptAdvanced = gptJson->add_member(ptRoot, "iridescence");
        gptJson->add_float_member(ptAdvanced, "factor", ptMaterial->tIridescence.fFactor);
        gptJson->add_float_member(ptAdvanced, "ior", ptMaterial->tIridescence.fIor);
        gptJson->add_float_member(ptAdvanced, "thickness_min", ptMaterial->tIridescence.fThicknessMin);
        gptJson->add_float_member(ptAdvanced, "thickness_max", ptMaterial->tIridescence.fThicknessMax);
    }

    // anisotropy stuff
    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_ANISOTROPY)
    {
        plJsonObject* ptAdvanced = gptJson->add_member(ptRoot, "anisotropy");
        gptJson->add_float_member(ptAdvanced, "strength", ptMaterial->tAnisotropy.fStrength);
        gptJson->add_float_member(ptAdvanced, "rotation", ptMaterial->tAnisotropy.fRotation);
    }

    // transmission stuff
    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_TRANSMISSION)
    {
        plJsonObject* ptAdvanced = gptJson->add_member(ptRoot, "transmission");
        gptJson->add_float_member(ptAdvanced, "factor", ptMaterial->tTransmission.fFactor);
    }

    // volume stuff
    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_VOLUME)
    {
        plJsonObject* ptAdvanced = gptJson->add_member(ptRoot, "volume");
        gptJson->add_float_member(ptAdvanced, "thickness", ptMaterial->tVolume.fThickness);
        gptJson->add_float_member(ptAdvanced, "attenuation_distance", ptMaterial->tVolume.fAttenuationDistance);
        gptJson->add_float_array(ptAdvanced, "attenuation_color", ptMaterial->tVolume.tAttenuationColor.d, 3);
    }

    // dispersion stuff
    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_DISPERSION)
    {
        plJsonObject* ptAdvanced = gptJson->add_member(ptRoot, "dispersion");
        gptJson->add_float_member(ptAdvanced, "dispersion", ptMaterial->tDispersion.fDispersion);
    }

    // diffuse transmission stuff
    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_DIFFUSE_TRANSMISSION)
    {
        plJsonObject* ptAdvanced = gptJson->add_member(ptRoot, "diffuse_transmission");
        gptJson->add_float_member(ptAdvanced, "factor", ptMaterial->tDiffuseTransmission.fFactor);
        gptJson->add_float_array(ptAdvanced, "color", ptMaterial->tDiffuseTransmission.tColor.d, 3);
    }

    // emissive stuff
    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_EMISSIVE)
    {
        plJsonObject* ptEmissive = gptJson->add_member(ptRoot, "emissive");
        gptJson->add_float_array(ptEmissive, "color", ptMaterial->tEmissiveColor.d, 3);
        gptJson->add_float_member(ptEmissive, "strength", ptMaterial->fEmissiveStrength);
    }

    // textures stuff
    uint32_t uTextureCount = 0;
    for(uint32_t i = 0; i < PL_MATERIAL_TEXTURE_SLOT_COUNT; i++)
    {
        if(gptAsset->is_valid(ptMaterial->atTextures[i].tTexture))
            uTextureCount++;
    }

    if(uTextureCount > 0)
    {
        plJsonObject* ptTextures = gptJson->add_member(ptRoot, "textures");
        for(uint32_t i = 0; i < PL_MATERIAL_TEXTURE_SLOT_COUNT; i++)
        {
            if(gptAsset->is_valid(ptMaterial->atTextures[i].tTexture))
            {
                plJsonObject* ptTexture = gptJson->add_member(ptTextures, gapcTextureSlotNames[i]);
                gptJson->add_string_member(ptTexture, "resource", gptAsset->get_path(ptMaterial->atTextures[i].tTexture));
                gptJson->add_uint32_member(ptTexture, "uv_set", ptMaterial->atTextures[i].uUVSet);
                gptJson->add_float_array(ptTexture, "scale", ptMaterial->atTextures[i].tScale.d, 2);
                gptJson->add_float_array(ptTexture, "offset", ptMaterial->atTextures[i].tOffset.d, 2);
                gptJson->add_float_member(ptTexture, "rotation", ptMaterial->atTextures[i].fRotation);
            }
        }
    }

    uint32_t uBufferSize = 0;
    gptJson->write(ptRoot, NULL, &uBufferSize);
    char* pcBuffer = PL_ALLOC(uBufferSize);
    memset(pcBuffer, 0, uBufferSize);
    gptJson->write(ptRoot, pcBuffer, &uBufferSize);
    
    gptVfs->register_file(pcName, false);
    plVfsFileHandle tFileHandle = gptVfs->open_file(pcName, PL_VFS_FILE_MODE_WRITE);
    gptVfs->write_file(tFileHandle, pcBuffer, uBufferSize);
    gptVfs->close_file(tFileHandle);

    PL_FREE(pcBuffer);
    gptJson->unload(&ptRoot);
    return true;
}

static bool
pl__material_deserialize(const char* pcName, void* pMaterial)
{
    plMaterial* ptMaterial = pMaterial;

    if(!gptVfs->does_file_exist(pcName))
        return false;

    size_t szJsonFileSize = gptVfs->get_file_size_str(pcName);
    uint8_t* puFileBuffer = (uint8_t*)PL_ALLOC(szJsonFileSize + 1);
    memset(puFileBuffer, 0, szJsonFileSize + 1);

    plVfsFileHandle tFileHandle = gptVfs->open_file(pcName, PL_VFS_FILE_MODE_READ);
    gptVfs->read_file(tFileHandle, puFileBuffer, &szJsonFileSize);
    gptVfs->close_file(tFileHandle);

    char acTempBuffer[256] = {0};

    plJsonObject* ptRoot = NULL;
    gptJson->load((const char*)puFileBuffer, &ptRoot);

    uint32_t uVersion = gptJson->uint32_member(ptRoot, "version", 0);

    pl_material_init(ptMaterial);
    plMaterialWater tWater = gtMaterialWaterDefault;

    // base material stuff
    gptJson->float_array_member(ptRoot, "base_color", ptMaterial->tBaseColor.d, NULL);
    ptMaterial->fMetalness = gptJson->float_member(ptRoot, "metalness", ptMaterial->fMetalness);
    ptMaterial->fRoughness = gptJson->float_member(ptRoot, "roughness", ptMaterial->fRoughness);
    ptMaterial->fIor = gptJson->float_member(ptRoot, "ior", ptMaterial->fIor);
    ptMaterial->fNormalMapStrength = gptJson->float_member(ptRoot, "normal_map_strength", ptMaterial->fNormalMapStrength);
    ptMaterial->fOcclusionStrength = gptJson->float_member(ptRoot, "occlusion_strength", ptMaterial->fOcclusionStrength);

    if(gptJson->bool_member(ptRoot, "double_sided", (bool)(ptMaterial->eFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED)))
        ptMaterial->eFlags |= PL_MATERIAL_FLAG_DOUBLE_SIDED;

    if(gptJson->bool_member(ptRoot, "foliage_normals", (bool)(ptMaterial->eFlags & PL_MATERIAL_FLAG_FOLIAGE_NORMALS)))
        ptMaterial->eFlags |= PL_MATERIAL_FLAG_FOLIAGE_NORMALS;

    strncpy(acTempBuffer, "pbr_metallic_roughness", 256);
    gptJson->string_member(ptRoot, "model", acTempBuffer, 256);
    if     (strcmp(acTempBuffer, "water") == 0) ptMaterial->eMaterialModel = PL_MATERIAL_MODEL_WATER;
    else if(strcmp(acTempBuffer, "pbr_metallic_roughness_foliage") == 0) ptMaterial->eMaterialModel = PL_MATERIAL_MODEL_PBR_METALLIC_ROUGHNESS_FOLIAGE;
    else                             ptMaterial->eMaterialModel = PL_MATERIAL_MODEL_PBR_METALLIC_ROUGHNESS;

    plJsonObject* ptWater = gptJson->member(ptRoot, "water");
    if(ptWater)
    {
        char acWaveMode[32] = "swell";
        gptJson->string_member(ptWater, "wave_mode", acWaveMode, 32);
        tWater.iWaveMode = strcmp(acWaveMode, "fft") == 0 ? PL_WATER_WAVE_MODE_FFT : (strcmp(acWaveMode, "gerstner") == 0 ? PL_WATER_WAVE_MODE_GERSTNER : PL_WATER_WAVE_MODE_SWELL);
        tWater.iFftResolution = gptJson->int_member(ptWater, "fft_resolution", tWater.iFftResolution);
        tWater.iFftSeed = gptJson->int_member(ptWater, "fft_seed", tWater.iFftSeed);
        tWater.fGerstnerSteepness = gptJson->float_member(ptWater, "gerstner_steepness", tWater.fGerstnerSteepness);
        tWater.fFftPatchSize = gptJson->float_member(ptWater, "fft_patch_size", tWater.fFftPatchSize);
        tWater.fFftHeight = gptJson->float_member(ptWater, "fft_height_limit", tWater.fFftHeight);
        tWater.fFftWindSpeed = gptJson->float_member(ptWater, "fft_wind_speed", tWater.fFftWindSpeed);
        tWater.fFftWindHeading = gptJson->float_member(ptWater, "fft_wind_heading_degrees", tWater.fFftWindHeading);
        tWater.fFftWindDependency = gptJson->float_member(ptWater, "fft_against_wind", tWater.fFftWindDependency);
        tWater.fFftDamping = gptJson->float_member(ptWater, "fft_short_wave_damping", tWater.fFftDamping);
        tWater.fFftChoppiness = gptJson->float_member(ptWater, "fft_choppiness", tWater.fFftChoppiness);
        tWater.fFftSpeed = gptJson->float_member(ptWater, "fft_speed", tWater.fFftSpeed);
        tWater.fAmplitude0 = gptJson->float_member(ptWater, "swell0_amplitude", tWater.fAmplitude0);
        tWater.fWavelength0 = gptJson->float_member(ptWater, "swell0_wavelength", tWater.fWavelength0);
        tWater.fSpeed0 = gptJson->float_member(ptWater, "swell0_speed", tWater.fSpeed0);
        tWater.fHeading0 = gptJson->float_member(ptWater, "swell0_heading_degrees", tWater.fHeading0);
        tWater.fAmplitude1 = gptJson->float_member(ptWater, "swell1_amplitude", tWater.fAmplitude1);
        tWater.fWavelength1 = gptJson->float_member(ptWater, "swell1_wavelength", tWater.fWavelength1);
        tWater.fSpeed1 = gptJson->float_member(ptWater, "swell1_speed", tWater.fSpeed1);
        tWater.fHeading1 = gptJson->float_member(ptWater, "swell1_heading_degrees", tWater.fHeading1);
        tWater.fDetailScale = gptJson->float_member(ptWater, "detail_scale", tWater.fDetailScale);
        tWater.fDetailSpeed = gptJson->float_member(ptWater, "detail_speed", tWater.fDetailSpeed);
        tWater.fFoamSpeed = gptJson->float_member(ptWater, "foam_speed", tWater.fFoamSpeed);
        tWater.fFoamScale = gptJson->float_member(ptWater, "foam_scale", tWater.fFoamScale);
        tWater.fShoreStrength = gptJson->float_member(ptWater, "shore_strength", tWater.fShoreStrength);
        tWater.fShoreDepth = gptJson->float_member(ptWater, "shore_depth", tWater.fShoreDepth);
        tWater.fShoreRayStart = gptJson->float_member(ptWater, "shore_ray_fade_start", tWater.fShoreRayStart);
        tWater.fShoreRayEnd = gptJson->float_member(ptWater, "shore_ray_fade_end", tWater.fShoreRayEnd);
        tWater.fWhitecapStrength = gptJson->float_member(ptWater, "whitecap_strength", tWater.fWhitecapStrength);
        tWater.fCrestStart = gptJson->float_member(ptWater, "crest_start", tWater.fCrestStart);
        tWater.fCrestFull = gptJson->float_member(ptWater, "crest_full", tWater.fCrestFull);
        tWater.fSlopeStart = gptJson->float_member(ptWater, "slope_start", tWater.fSlopeStart);
        tWater.fSlopeFull = gptJson->float_member(ptWater, "slope_full", tWater.fSlopeFull);
        tWater.fMinRoughness = gptJson->float_member(ptWater, "minimum_roughness", tWater.fMinRoughness);
        tWater.fFoamRoughness = gptJson->float_member(ptWater, "foam_roughness", tWater.fFoamRoughness);
        tWater.fFoamNormalFlatten = gptJson->float_member(ptWater, "foam_normal_flatten", tWater.fFoamNormalFlatten);
        tWater.fDistortionStrength = gptJson->float_member(ptWater, "refraction_strength", tWater.fDistortionStrength);
        tWater.fMaxDistortionPixels = gptJson->float_member(ptWater, "refraction_max_pixels", tWater.fMaxDistortionPixels);
        tWater.fAbsorptionRed = gptJson->float_member(ptWater, "absorption_red", tWater.fAbsorptionRed);
        tWater.fAbsorptionGreen = gptJson->float_member(ptWater, "absorption_green", tWater.fAbsorptionGreen);
        tWater.fAbsorptionBlue = gptJson->float_member(ptWater, "absorption_blue", tWater.fAbsorptionBlue);
        tWater.fAbsorptionDistance = gptJson->float_member(ptWater, "absorption_distance", tWater.fAbsorptionDistance);
        tWater.fScatteringRed = gptJson->float_member(ptWater, "scattering_red", tWater.fScatteringRed);
        tWater.fScatteringGreen = gptJson->float_member(ptWater, "scattering_green", tWater.fScatteringGreen);
        tWater.fScatteringBlue = gptJson->float_member(ptWater, "scattering_blue", tWater.fScatteringBlue);
        tWater.fScatteringStrength = gptJson->float_member(ptWater, "scattering_strength", tWater.fScatteringStrength);
        tWater.fScatteringDirection = gptJson->float_member(ptWater, "scattering_direction", tWater.fScatteringDirection);
        tWater.fMaxPath = gptJson->float_member(ptWater, "refraction_max_distance", tWater.fMaxPath);
        tWater.fOpacityDistance = gptJson->float_member(ptWater, "opacity_distance", tWater.fOpacityDistance);
        tWater.fEdgeFade = gptJson->float_member(ptWater, "refraction_edge_fade_pixels", tWater.fEdgeFade);
        tWater.fIntersectionFade = gptJson->float_member(ptWater, "refraction_intersection_fade", tWater.fIntersectionFade);
        tWater.fFoamRed = gptJson->float_member(ptWater, "foam_red", tWater.fFoamRed);
        tWater.fFoamGreen = gptJson->float_member(ptWater, "foam_green", tWater.fFoamGreen);
        tWater.fFoamBlue = gptJson->float_member(ptWater, "foam_blue", tWater.fFoamBlue);
        tWater.fDebugView = gptJson->float_member(ptWater, "debug_view", tWater.fDebugView);
    }
    // Old water materials retain their scalar extinction through neutral absorption.
    bool bHasVolume = false;
    if(ptWater)
    {
        bHasVolume |= gptJson->member(ptWater, "absorption_red") != NULL;
        bHasVolume |= gptJson->member(ptWater, "absorption_green") != NULL;
        bHasVolume |= gptJson->member(ptWater, "absorption_blue") != NULL;
        bHasVolume |= gptJson->member(ptWater, "absorption_distance") != NULL;
        bHasVolume |= gptJson->member(ptWater, "scattering_red") != NULL;
        bHasVolume |= gptJson->member(ptWater, "scattering_green") != NULL;
        bHasVolume |= gptJson->member(ptWater, "scattering_blue") != NULL;
        bHasVolume |= gptJson->member(ptWater, "scattering_strength") != NULL;
        bHasVolume |= gptJson->member(ptWater, "scattering_direction") != NULL;
    }
    if(ptMaterial->eMaterialModel == PL_MATERIAL_MODEL_WATER && !bHasVolume)
    {
        float fTransmission = 1.0f - ptMaterial->tBaseColor.a;
        if(!isfinite(fTransmission)) fTransmission = 0.35f;
        fTransmission = fminf(1.0f, fmaxf(0.0001f, fTransmission));
        tWater.fAbsorptionRed = fTransmission;
        tWater.fAbsorptionGreen = fTransmission;
        tWater.fAbsorptionBlue = fTransmission;
        tWater.fAbsorptionDistance = tWater.fOpacityDistance;
        tWater.fScatteringStrength = 0.0f;
    }

    // alpha stuff
    plJsonObject* ptAlpha = gptJson->member(ptRoot, "alpha");
    if(ptAlpha)
    {
        ptMaterial->fAlphaCutoff = gptJson->float_member(ptAlpha, "cutoff", ptMaterial->fAlphaCutoff);
    
        strncpy(acTempBuffer, "opaque", 256);
        gptJson->string_member(ptAlpha, "mode", acTempBuffer, 256);
        if     (acTempBuffer[0] == 'm') ptMaterial->eAlphaMode = PL_MATERIAL_ALPHA_MODE_MASK;
        else if(acTempBuffer[0] == 'b') ptMaterial->eAlphaMode = PL_MATERIAL_ALPHA_MODE_BLEND;
        else if(acTempBuffer[0] == 'o') ptMaterial->eAlphaMode = PL_MATERIAL_ALPHA_MODE_OPAQUE;
    }

    // clearcoat stuff
    {
        plJsonObject* ptAdvanced = gptJson->member(ptRoot, "clearcoat");
        if(ptAdvanced)
        {
            ptMaterial->eFlags |= PL_MATERIAL_FLAG_CLEARCOAT;
            ptMaterial->tClearcoat.fFactor = gptJson->float_member(ptAdvanced, "factor", ptMaterial->tClearcoat.fFactor);
            ptMaterial->tClearcoat.fRoughness = gptJson->float_member(ptAdvanced, "roughness", ptMaterial->tClearcoat.fRoughness);
            ptMaterial->tClearcoat.fNormalMapStrength = gptJson->float_member(ptAdvanced, "normal_map_strength", ptMaterial->tClearcoat.fNormalMapStrength);
        }
    }

    // sheen stuff
    {
        plJsonObject* ptAdvanced = gptJson->member(ptRoot, "sheen");
        if(ptAdvanced)
        {
            ptMaterial->eFlags |= PL_MATERIAL_FLAG_SHEEN;
            gptJson->float_array_member(ptAdvanced, "color", ptMaterial->tSheen.tColor.d, NULL);
            ptMaterial->tSheen.fRoughness = gptJson->float_member(ptAdvanced, "roughness", ptMaterial->tSheen.fRoughness);
        }
    }

    // iridescence stuff
    {
        plJsonObject* ptAdvanced = gptJson->member(ptRoot, "iridescence");
        if(ptAdvanced)
        {
            ptMaterial->eFlags |= PL_MATERIAL_FLAG_IRIDESCENCE;
            ptMaterial->tIridescence.fFactor = gptJson->float_member(ptAdvanced, "factor", ptMaterial->tIridescence.fFactor);
            ptMaterial->tIridescence.fIor = gptJson->float_member(ptAdvanced, "ior", ptMaterial->tIridescence.fIor);
            ptMaterial->tIridescence.fThicknessMin = gptJson->float_member(ptAdvanced, "thickness_min", ptMaterial->tIridescence.fThicknessMin);
            ptMaterial->tIridescence.fThicknessMax = gptJson->float_member(ptAdvanced, "thickness_max", ptMaterial->tIridescence.fThicknessMax);
        }
    }

    // anisotropy stuff
    {
        plJsonObject* ptAdvanced = gptJson->member(ptRoot, "anisotropy");
        if(ptAdvanced)
        {
            ptMaterial->eFlags |= PL_MATERIAL_FLAG_ANISOTROPY;
            ptMaterial->tAnisotropy.fStrength = gptJson->float_member(ptAdvanced, "strength", ptMaterial->tAnisotropy.fStrength);
            ptMaterial->tAnisotropy.fRotation = gptJson->float_member(ptAdvanced, "rotation", ptMaterial->tAnisotropy.fRotation);
        }
    }

    // transmission stuff
    {
        plJsonObject* ptAdvanced = gptJson->member(ptRoot, "transmission");
        if(ptAdvanced)
        {
            ptMaterial->eFlags |= PL_MATERIAL_FLAG_TRANSMISSION;
            ptMaterial->tTransmission.fFactor = gptJson->float_member(ptAdvanced, "factor", ptMaterial->tTransmission.fFactor);
        }
    }

    // volume stuff
    {
        plJsonObject* ptAdvanced = gptJson->member(ptRoot, "volume");
        if(ptAdvanced)
        {
            ptMaterial->eFlags |= PL_MATERIAL_FLAG_VOLUME;
            ptMaterial->tVolume.fThickness = gptJson->float_member(ptAdvanced, "thickness", ptMaterial->tVolume.fThickness);
            ptMaterial->tVolume.fAttenuationDistance = gptJson->float_member(ptAdvanced, "attenuation_distance", ptMaterial->tVolume.fAttenuationDistance);
            gptJson->float_array_member(ptAdvanced, "attenuation_color", ptMaterial->tVolume.tAttenuationColor.d, NULL);
        }
    }

    // dispersion stuff
    {
        plJsonObject* ptAdvanced = gptJson->member(ptRoot, "dispersion");
        if(ptAdvanced)
        {
            ptMaterial->eFlags |= PL_MATERIAL_FLAG_DISPERSION;
            ptMaterial->tDispersion.fDispersion = gptJson->float_member(ptAdvanced, "dispersion", ptMaterial->tDispersion.fDispersion);
        }
    }

    // diffuse transmission stuff
    {
        plJsonObject* ptAdvanced = gptJson->member(ptRoot, "diffuse_transmission");
        if(ptAdvanced)
        {
            ptMaterial->eFlags |= PL_MATERIAL_FLAG_DIFFUSE_TRANSMISSION;
            ptMaterial->tDiffuseTransmission.fFactor = gptJson->float_member(ptAdvanced, "factor", ptMaterial->tDiffuseTransmission.fFactor);
            gptJson->float_array_member(ptAdvanced, "color", ptMaterial->tDiffuseTransmission.tColor.d, NULL);
        }
    }

    // emissive stuff
    {
        plJsonObject* ptAdvanced = gptJson->member(ptRoot, "emissive");
        if(ptAdvanced)
        {
            ptMaterial->eFlags |= PL_MATERIAL_FLAG_EMISSIVE;
            ptMaterial->fEmissiveStrength = gptJson->float_member(ptAdvanced, "strength", ptMaterial->fEmissiveStrength);
            gptJson->float_array_member(ptAdvanced, "color", ptMaterial->tEmissiveColor.d, NULL);
        }
    }

    plJsonObject* ptTextures = gptJson->member(ptRoot, "textures");
    if(ptTextures)
    {
        uint32_t uTextureCount = UINT32_MAX;
        gptJson->member_list(ptTextures, NULL, &uTextureCount, NULL);
        for(uint32_t i = 0; i < uTextureCount; i++)
        {
            plJsonObject* ptTexture = gptJson->member_by_index(ptTextures, i);
            const char* pcTextureName = gptJson->get_name(ptTexture);

            plMaterialTextureSlot tTextureSlot = 0;
            if     (pcTextureName[0] == 'b') tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_BASE_COLOR;
            else if(pcTextureName[0] == 'n') tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_NORMAL;
            else if(pcTextureName[0] == 'e') tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_EMISSIVE;
            else if(pcTextureName[0] == 'o') tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_OCCLUSION;
            else if(pcTextureName[0] == 'm') tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_METAL_ROUGHNESS;
            else if(pcTextureName[0] == 'a') tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_ANISOTROPY;
            else if(pcTextureName[0] == 'c')
            {
                if     (pcTextureName[10] == 'r') tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_CLEARCOAT_ROUGHNESS;
                else if(pcTextureName[10] == 'n') tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_CLEARCOAT_NORMAL;
                else                              tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_CLEARCOAT;
            }
            else if(pcTextureName[0] == 's')
            {
                if     (pcTextureName[6] == 'c') tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_SHEEN_COLOR;
                else if(pcTextureName[6] == 'r') tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_SHEEN_ROUGHNESS;
            }
            else if(pcTextureName[0] == 'i')
            {
                if     (pcTextureName[12] == 't') tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_IRIDESCENCE_THICKNESS;
                else                              tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_IRIDESCENCE;
            }
            else if(pcTextureName[0] == 't')
            {
                if     (pcTextureName[13] == 'c') tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_TRANSMISSION;
                else if(pcTextureName[13] == 't') tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_THICKNESS;
            }
            else if(pcTextureName[0] == 'd')
            {
                if     (pcTextureName[21] == 'c') tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_DIFFUSE_TRANSMISSION_COLOR;
                else                              tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_DIFFUSE_TRANSMISSION;
            }

            ptMaterial->atTextures[tTextureSlot].uUVSet = gptJson->uint32_member(ptTexture, "uv_set", ptMaterial->atTextures[tTextureSlot].uUVSet);
            ptMaterial->atTextures[tTextureSlot].fRotation = gptJson->float_member(ptTexture, "rotation", ptMaterial->atTextures[tTextureSlot].fRotation);
            gptJson->float_array_member(ptTexture, "scale", ptMaterial->atTextures[tTextureSlot].tScale.d, NULL);
            gptJson->float_array_member(ptTexture, "offset", ptMaterial->atTextures[tTextureSlot].tOffset.d, NULL);
            gptJson->string_member(ptTexture, "resource", acTempBuffer, 256);
            ptMaterial->atTextures[tTextureSlot].tTexture = gptAsset->load(acTempBuffer);
        }
    }

    PL_FREE(puFileBuffer);
    gptJson->unload(&ptRoot);

    ptMaterial->tWater = tWater;
    *ptMaterial = pl_material_sanitize(*ptMaterial);

    return true;
}

void
pl_material_register_asset_types(void)
{
    static const plAssetTypeDesc tDesc = {
        .pcName          = "Material",
        .pcFileExtension = "plmaterial",
        .szSize          = sizeof(plMaterial),
        .serialize       = pl__material_serialize,
        .deserialize     = pl__material_deserialize,
    };
    gptMaterialCtx->tAssetTypeKey = gptAsset->register_type(tDesc);
}

plAssetTypeKey
pl_material_get_asset_type_key(void)
{
    return gptMaterialCtx->tAssetTypeKey;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_material_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plMaterialI tApi = {
        .init                 = pl_material_init,
        .sanitize             = pl_material_sanitize,
        .register_asset_types = pl_material_register_asset_types,
        .get_asset_type_key   = pl_material_get_asset_type_key,
    };
    pl_set_api(ptApiRegistry, plMaterialI, &tApi);

    #ifndef PL_UNITY_BUILD
    gptVfs    = pl_get_api_latest(ptApiRegistry, plVfsI);
    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptAsset  = pl_get_api_latest(ptApiRegistry, plAssetI);
    gptJson  = pl_get_api_latest(ptApiRegistry, plJsonI);
    #endif

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptMaterialCtx = ptDataRegistry->get_data("plMaterialContext");
    }
    else // first load
    {
        static plMaterialContext tCtx = {0};
        gptMaterialCtx = &tCtx;
        ptDataRegistry->set_data("plMaterialContext", gptMaterialCtx);
    }
}

void
pl_unload_material_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plMaterialI* ptApi = pl_get_api_latest(ptApiRegistry, plMaterialI);
    ptApiRegistry->remove_api(ptApi);
}
