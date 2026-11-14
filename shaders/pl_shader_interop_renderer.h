/*
   pl_shader_interop_renderer.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] defines
// [SECTION] enums
// [SECTION] common
// [SECTION] scene
// [SECTION] view
// [SECTION] grid
// [SECTION] post processing
// [SECTION] environment filtering
// [SECTION] shadows
// [SECTION] skinning
// [SECTION] skybox
// [SECTION] outlines
// [SECTION] picking
// [SECTION] lights
// [SECTION] materials
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_SHADER_INTEROP_RENDERER_H
#define PL_SHADER_INTEROP_RENDERER_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_shader_interop.h"

// Byte comments give each group size and its offset from the start of the struct.
// Totals match the C layout and std430 array stride; scalar arrays require std430.

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#define PL_MAX_DECALS 64
#define PL_MAX_LIGHTS 100
#define PL_CLUSTER_X 16
#define PL_CLUSTER_Y 9
#define PL_CLUSTER_Z 24
#define PL_CLUSTER_LIGHT_CAPACITY 64
#define PL_CLUSTER_STRIDE (2 + 2 * PL_CLUSTER_LIGHT_CAPACITY)
#define PL_CLUSTER_BUFFER_SIZE (PL_CLUSTER_X * PL_CLUSTER_Y * PL_CLUSTER_Z * PL_CLUSTER_STRIDE * 4)

#define PL_MAX_BINDLESS_TEXTURES 4096
#define PL_MAX_BINDLESS_TEXTURE_SLOT 8
#define PL_MAX_BINDLESS_CUBE_TEXTURE_SLOT PL_MAX_BINDLESS_TEXTURES + PL_MAX_BINDLESS_TEXTURE_SLOT

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

PL_BEGIN_ENUM(plShaderAlphaMode)
    PL_ENUM_ITEM(PL_SHADER_ALPHA_MODE_OPAQUE, 0)
    PL_ENUM_ITEM(PL_SHADER_ALPHA_MODE_MASK, 1)
    PL_ENUM_ITEM(PL_SHADER_ALPHA_MODE_BLEND, 2)
PL_END_ENUM

PL_BEGIN_ENUM(plMaterialShaderFlags)
    PL_ENUM_ITEM(PL_MATERIAL_SHADER_FLAG_CLEARCOAT,            1 << 0)
    PL_ENUM_ITEM(PL_MATERIAL_SHADER_FLAG_SHEEN,                1 << 1)
    PL_ENUM_ITEM(PL_MATERIAL_SHADER_FLAG_IRIDESCENCE,          1 << 2)
    PL_ENUM_ITEM(PL_MATERIAL_SHADER_FLAG_ANISOTROPY,           1 << 3)
    PL_ENUM_ITEM(PL_MATERIAL_SHADER_FLAG_TRANSMISSION,         1 << 4)
    PL_ENUM_ITEM(PL_MATERIAL_SHADER_FLAG_VOLUME,               1 << 5)
    PL_ENUM_ITEM(PL_MATERIAL_SHADER_FLAG_DISPERSION,           1 << 6)
    PL_ENUM_ITEM(PL_MATERIAL_SHADER_FLAG_DIFFUSE_TRANSMISSION, 1 << 7)
    PL_ENUM_ITEM(PL_MATERIAL_SHADER_FLAG_EMISSIVE,             1 << 8)
    PL_ENUM_ITEM(PL_MATERIAL_SHADER_FLAG_FOLIAGE_NORMALS,      1 << 9)
    PL_ENUM_ITEM(PL_MATERIAL_SHADER_FLAG_WATER,                1 << 10)
PL_END_ENUM

PL_BEGIN_ENUM(plSceneFlags)
    PL_ENUM_ITEM(PL_SCENE_FLAG_HEIGHT_FOG,          1 << 0)
    PL_ENUM_ITEM(PL_SCENE_FLAG_LINEAR_FOG,          1 << 1)
    PL_ENUM_ITEM(PL_SCENE_FLAG_USE_IBL,             1 << 2)
    PL_ENUM_ITEM(PL_SCENE_FLAG_SHADOWS,             1 << 3)
    PL_ENUM_ITEM(PL_SCENE_FLAG_USE_NORMAL_MAPS,     1 << 4)
    PL_ENUM_ITEM(PL_SCENE_FLAG_PCF_SHADOWS,         1 << 5)
    PL_ENUM_ITEM(PL_SCENE_FLAG_PUNCTUAL,            1 << 6)
    PL_ENUM_ITEM(PL_SCENE_FLAG_SUN_CONTACT_SHADOWS, 1 << 7)
PL_END_ENUM

PL_BEGIN_ENUM(plObjectShaderFlags)
    PL_ENUM_ITEM(PL_OBJECT_SHADER_FLAG_PROBE,   1 << 0)
    PL_ENUM_ITEM(PL_OBJECT_SHADER_FLAG_SHADOWS, 1 << 1)
PL_END_ENUM

PL_BEGIN_ENUM(plMeshFormatFlags)
    PL_ENUM_ITEM(PL_MESH_FORMAT_FLAG_NONE,                     0)
    PL_ENUM_ITEM(PL_MESH_FORMAT_FLAG_HAS_NORMAL,         1 <<  0) 
    PL_ENUM_ITEM(PL_MESH_FORMAT_FLAG_HAS_TANGENT,        1 <<  1) 
    PL_ENUM_ITEM(PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0,     1 <<  2) 
    PL_ENUM_ITEM(PL_MESH_FORMAT_FLAG_HAS_COLOR,          1 <<  3)
    PL_ENUM_ITEM(PL_MESH_FORMAT_FLAG_HAS_JOINTS_0,       1 <<  4) 
    PL_ENUM_ITEM(PL_MESH_FORMAT_FLAG_HAS_JOINTS_1,       1 <<  5)
    PL_ENUM_ITEM(PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0,      1 <<  6)
    PL_ENUM_ITEM(PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1,      1 <<  7)
PL_END_ENUM

PL_BEGIN_ENUM(plTonemapMode)
    PL_ENUM_ITEM(PL_TONEMAP_MODE_NONE,                     0)
    PL_ENUM_ITEM(PL_TONEMAP_MODE_SIMPLE,                   1)
    PL_ENUM_ITEM(PL_TONEMAP_MODE_ACES_NARKOWICZ,           2)
    PL_ENUM_ITEM(PL_TONEMAP_MODE_ACES_HILL,                3)
    PL_ENUM_ITEM(PL_TONEMAP_MODE_ACES_HILL_EXPOSURE_BOOST, 4)
    PL_ENUM_ITEM(PL_TONEMAP_MODE_REINHARD,                 5)
    PL_ENUM_ITEM(PL_TONEMAP_MODE_KHRONOS_PBR_NEUTRAL,      6)
PL_END_ENUM

PL_BEGIN_ENUM(plLightType)
    PL_ENUM_ITEM(PL_LIGHT_TYPE_DIRECTIONAL, 0)
    PL_ENUM_ITEM(PL_LIGHT_TYPE_POINT,       1)
    PL_ENUM_ITEM(PL_LIGHT_TYPE_SPOT,        2)
PL_END_ENUM

PL_BEGIN_ENUM(plShaderDebugMode)
    PL_ENUM_ITEM(PL_SHADER_DEBUG_MODE_NONE, 0)
    PL_ENUM_ITEM(PL_SHADER_DEBUG_BASE_COLOR, 1)
    PL_ENUM_ITEM(PL_SHADER_DEBUG_METALLIC, 2)
    PL_ENUM_ITEM(PL_SHADER_DEBUG_ROUGHNESS, 3)
    PL_ENUM_ITEM(PL_SHADER_DEBUG_ALPHA, 4)
    PL_ENUM_ITEM(PL_SHADER_DEBUG_EMMISSIVE, 5)
    PL_ENUM_ITEM(PL_SHADER_DEBUG_OCCLUSION, 6)
    PL_ENUM_ITEM(PL_SHADER_DEBUG_SHADING_NORMAL, 7)
    PL_ENUM_ITEM(PL_SHADER_DEBUG_TEXTURE_NORMAL, 8)
    PL_ENUM_ITEM(PL_SHADER_DEBUG_GEOMETRY_NORMAL, 9)
    PL_ENUM_ITEM(PL_SHADER_DEBUG_GEOMETRY_TANGENT, 10)
    PL_ENUM_ITEM(PL_SHADER_DEBUG_GEOMETRY_BITANGENT, 11)
    PL_ENUM_ITEM(PL_SHADER_DEBUG_UV0, 12)
    PL_ENUM_ITEM(PL_SHADER_DEBUG_CLEARCOAT, 13)
    PL_ENUM_ITEM(PL_SHADER_DEBUG_CLEARCOAT_ROUGHNESS, 14)
    PL_ENUM_ITEM(PL_SHADER_DEBUG_CLEARCOAT_NORMAL, 15)
    PL_ENUM_ITEM(PL_SHADER_DEBUG_SHEEN_COLOR, 16)
    PL_ENUM_ITEM(PL_SHADER_DEBUG_SHEEN_ROUGHNESS, 17)
    PL_ENUM_ITEM(PL_SHADER_DEBUG_IRIDESCENCE_FACTOR, 18)
    PL_ENUM_ITEM(PL_SHADER_DEBUG_IRIDESCENCE_THICKNESS, 19)
    PL_ENUM_ITEM(PL_SHADER_DEBUG_ANISOTROPY_STRENGTH, 20)
    PL_ENUM_ITEM(PL_SHADER_DEBUG_ANISOTROPY_DIRECTION, 21)
    PL_ENUM_ITEM(PL_SHADER_DEBUG_TRANSMISSION_STRENGTH, 22)
    PL_ENUM_ITEM(PL_SHADER_DEBUG_VOLUME_THICKNESS, 23)
    PL_ENUM_ITEM(PL_SHADER_DEBUG_DIFFUSE_TRANSMISSION_STRENGTH, 24)
    PL_ENUM_ITEM(PL_SHADER_DEBUG_DIFFUSE_TRANSMISSION_COLOR, 25)
    PL_ENUM_ITEM(PL_SHADER_DEBUG_PROBE_SELECTION, 26)
PL_END_ENUM

PL_BEGIN_ENUM(plShaderTextureSlot)
    PL_ENUM_ITEM(PL_TEXTURE_BASE_COLOR,             0)
    PL_ENUM_ITEM(PL_TEXTURE_NORMAL,                 1)
    PL_ENUM_ITEM(PL_TEXTURE_EMISSIVE,               2)
    PL_ENUM_ITEM(PL_TEXTURE_OCCLUSION,              3)

    PL_ENUM_ITEM(PL_TEXTURE_METAL_ROUGHNESS,        4)
    PL_ENUM_ITEM(PL_TEXTURE_CLEARCOAT,              5)
    PL_ENUM_ITEM(PL_TEXTURE_CLEARCOAT_ROUGHNESS,    6)
    PL_ENUM_ITEM(PL_TEXTURE_CLEARCOAT_NORMAL,       7)

    PL_ENUM_ITEM(PL_TEXTURE_SHEEN_COLOR,            8)
    PL_ENUM_ITEM(PL_TEXTURE_SHEEN_ROUGHNESS,        9)
    PL_ENUM_ITEM(PL_TEXTURE_IRIDESCENCE,           10)
    PL_ENUM_ITEM(PL_TEXTURE_IRIDESCENCE_THICKNESS, 11)

    PL_ENUM_ITEM(PL_TEXTURE_ANISOTROPY,            12)
    PL_ENUM_ITEM(PL_TEXTURE_TRANSMISSION,          13)
    PL_ENUM_ITEM(PL_TEXTURE_THICKNESS,             14)
    PL_ENUM_ITEM(PL_TEXTURE_DIFFUSE_TRANSMISSION,  15)

    PL_ENUM_ITEM(PL_TEXTURE_DIFFUSE_TRANSMISSION_COLOR, 16)

    PL_ENUM_ITEM(PL_TEXTURE_COUNT, 20)
PL_END_ENUM

PL_BEGIN_ENUM(plTextureMappingFlags)
    PL_ENUM_ITEM(PL_HAS_BASE_COLOR_MAP,                 1 <<  0)
    PL_ENUM_ITEM(PL_HAS_NORMAL_MAP,                     1 <<  1)
    PL_ENUM_ITEM(PL_HAS_EMISSIVE_MAP,                   1 <<  2)
    PL_ENUM_ITEM(PL_HAS_OCCLUSION_MAP,                  1 <<  3)
    PL_ENUM_ITEM(PL_HAS_METALLIC_ROUGHNESS_MAP,         1 <<  4)
    PL_ENUM_ITEM(PL_HAS_CLEARCOAT_MAP,                  1 <<  5)
    PL_ENUM_ITEM(PL_HAS_CLEARCOAT_ROUGHNESS_MAP,        1 <<  6)
    PL_ENUM_ITEM(PL_HAS_CLEARCOAT_NORMAL_MAP,           1 <<  7)
    PL_ENUM_ITEM(PL_HAS_SHEEN_COLOR_MAP,                1 <<  8)
    PL_ENUM_ITEM(PL_HAS_SHEEN_ROUGHNESS_MAP,            1 <<  9)
    PL_ENUM_ITEM(PL_HAS_IRIDESCENCE_MAP,                1 << 10)
    PL_ENUM_ITEM(PL_HAS_IRIDESCENCE_THICKNESS_MAP,      1 << 11)
    PL_ENUM_ITEM(PL_HAS_ANISOTROPY_MAP,                 1 << 12)
    PL_ENUM_ITEM(PL_HAS_TRANSMISSION_MAP,               1 << 13)
    PL_ENUM_ITEM(PL_HAS_THICKNESS_MAP,                  1 << 14)
    PL_ENUM_ITEM(PL_HAS_DIFFUSE_TRANSMISSION_MAP,       1 << 15)
    PL_ENUM_ITEM(PL_HAS_DIFFUSE_TRANSMISSION_COLOR_MAP, 1 << 16)
PL_END_ENUM

//-----------------------------------------------------------------------------
// [SECTION] common
//-----------------------------------------------------------------------------

#define PL_MAX_PROBE_SELECTION_INDEX 2048
// Diagnostic flag in CPU/vertex data only; strip before table reads or ORM writes.
#define PL_PROBE_SELECTION_FALLBACK_FLAG 0x80000000u
#define PL_PROBE_SELECTION_TABLE_SLOT (PL_MAX_BINDLESS_CUBE_TEXTURE_SLOT + PL_MAX_BINDLESS_TEXTURES)

// Sorted nearest first. Unused indices are zero; iCount is authoritative.
PL_BEGIN_STRUCT(plGpuProbeSelection)
    int iProbe0;
    int iProbe1;
    int iProbe2;
    int iCount;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 0)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~16 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuProbeSelection)

PL_BEGIN_STRUCT(plGpuTransform)
    mat4 tTransform;
    // ~~~~~~~~~~~~~~~~64 bytes (offset 0)~~~~~~~~~~~~~~~~
    uint uProbeSelection;
    uint _uProbeSelectionPadding0;
    uint _uProbeSelectionPadding1;
    uint _uProbeSelectionPadding2;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 64)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~80 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuTransform)

PL_BEGIN_STRUCT(plGpuDynData)
    int  iDataOffset;
    int  iVertexOffset;
    int  iMaterialIndex;
    uint uGlobalIndex;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 0)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~16 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuDynData)

PL_BEGIN_STRUCT(plGpuDynForwardData)
    int  iDataOffset;
    int  iVertexOffset;
    int  iMaterialIndex;
    uint uGlobalIndex;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 0)~~~~~~~~~~~~~~~~

    int  _iUnusedProbeCount;
    int  iPointLightCount;
    int  iSpotLightCount;
    int  iDirectionLightCount;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 16)~~~~~~~~~~~~~~~~

    int _iUnused0;
    int _iUnused1;
    int _iUnused2;
    int iObjectShaderFlags;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 32)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~48 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuDynForwardData)

//-----------------------------------------------------------------------------
// [SECTION] scene
//-----------------------------------------------------------------------------

PL_BEGIN_STRUCT(plGpuDecal)
    mat4 tWorldToDecal;
    // ~~~~~~~~~~~~~~~~64 bytes (offset 0)~~~~~~~~~~~~~~~~
    vec4 tNormalOpacity; // projector outward normal and opacity
    // ~~~~~~~~~~~~~~~~16 bytes (offset 64)~~~~~~~~~~~~~~~~
    int iMaterialIndex;
    int iSortOrder;
    float fNormalCutoff;
    float fEdgeFade;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 80)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~96 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuDecal)

PL_BEGIN_STRUCT(plGpuSceneData)

    vec3 tDirection; // main shadow-casting light direction (sun by day, moon by night)
    int iCascadeCount;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 0)~~~~~~~~~~~~~~~~

    vec3 tColor;
    int iCastShadow;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 16)~~~~~~~~~~~~~~~~

    int iShadowMapTexIdx;
    float fFactor;
    float fXOffset;
    float fYOffset;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 32)~~~~~~~~~~~~~~~~

    float fIntensity;
    float fSunRadius;
    float fAtmosphereConversion;
    int bCascadeDebug;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 48)~~~~~~~~~~~~~~~~

    int iBrdfLutIndex;
    int iSceneFlags;
    float fFogHeight;
    float fFogDensity;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 64)~~~~~~~~~~~~~~~~

    float fFogStart;
    float fFogCutOffDistance;
    float fFogMaxOpacity;
    float fFogHeightFalloff;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 80)~~~~~~~~~~~~~~~~

    vec3 tFogDensity;
    float fFogLinearParam0;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 96)~~~~~~~~~~~~~~~~

    vec3 tFogColor;
    float fFogLinearParam1;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 112)~~~~~~~~~~~~~~~~

    // atmosphere stuff
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    vec3 scatteringRayleighGround;
    float planetRadius;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 128)~~~~~~~~~~~~~~~~

    vec3 extinctionRayleighGround;
    float atmosphereHeight;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 144)~~~~~~~~~~~~~~~~

    vec3 ozoneExtinction;
    float scatteringMieGround;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 160)~~~~~~~~~~~~~~~~

    float extinctionMieGround;
    float mieScatteringExponent; // used in mie phase function
    float g_time;
    int bMultiScatter;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 176)~~~~~~~~~~~~~~~~

    // vec4 tSunColorRadius; // now "tColor" & "fSunRadius"
    // vec4 tSunDirection; // now "tDirection"
    // vec4 tCameraPos; // now in view data as "tCameraPos"

    // x = maximum aerial distance in atmosphere units
    // y = depth exponent
    // z = world-to-atmosphere scale
    // w = sun intensity
    vec4 tAerialInfo;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 192)~~~~~~~~~~~~~~~~
    vec4 tCloudLayer; // coverage (zero disables), base height, thickness, extinction
    // ~~~~~~~~~~~~~~~~16 bytes (offset 208)~~~~~~~~~~~~~~~~
    vec4 tCloudNoise; // frequency, wind X/Z, view step count
    // ~~~~~~~~~~~~~~~~16 bytes (offset 224)~~~~~~~~~~~~~~~~
    vec4 tCloudProfile; // reserved, multiple scattering strength, reserved, reserved
    // ~~~~~~~~~~~~~~~~16 bytes (offset 240)~~~~~~~~~~~~~~~~
    vec4 tCloudShadowCenter; // atmosphere-space center; bindless index (-1 = invalid)
    // ~~~~~~~~~~~~~~~~16 bytes (offset 256)~~~~~~~~~~~~~~~~
    vec4 tCloudShadowRight; // light-space basis and half extent
    // ~~~~~~~~~~~~~~~~16 bytes (offset 272)~~~~~~~~~~~~~~~~
    vec4 tCloudShadowUp;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 288)~~~~~~~~~~~~~~~~
    vec4 tCloudShadowForward;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 304)~~~~~~~~~~~~~~~~

    // Contact shadows: length, thickness, bias (world units), step count.
    vec4 tContactShadow;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 320)~~~~~~~~~~~~~~~~

    int iDecalCount;
    int _iDecalPadding0;
    int _iDecalPadding1;
    int _iDecalPadding2;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 336)~~~~~~~~~~~~~~~~
    plGpuDecal atDecals[PL_MAX_DECALS];
    // ~~~~~~~~~~~~~~~~6144 bytes (offset 352)~~~~~~~~~~~~~~~~

    vec4 tNightSky; // star intensity, star rotation, moon intensity, moon angular radius
    // ~~~~~~~~~~~~~~~~16 bytes (offset 6496)~~~~~~~~~~~~~~~~
    vec4 tMoonDirection; // direction toward moon; w = illuminated fraction
    // ~~~~~~~~~~~~~~~~16 bytes (offset 6512)~~~~~~~~~~~~~~~~
    vec4 tSunDirection; // actual solar light travel direction; w = 1 when main shadows belong to moon
    // ~~~~~~~~~~~~~~~~16 bytes (offset 6528)~~~~~~~~~~~~~~~~
    vec4 tSunRadiance; // actual solar color * intensity, independent of the main-light handoff
    // ~~~~~~~~~~~~~~~~16 bytes (offset 6544)~~~~~~~~~~~~~~~~
    vec4 tMoonlight; // lunar color * phase/horizon/twilight-adjusted intensity; w = moonlight enabled
    // ~~~~~~~~~~~~~~~~16 bytes (offset 6560)~~~~~~~~~~~~~~~~

    // mat4 tCameraProjectionInv; // camera
    // mat4 tInvViewMatNoTranslation; // camera
    // mat4 tShadowViewProj; // shadow camera
    // mat4 tCameraViewInv; // camera

    // ~~~~~~~~~~~~~~~~6576 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuSceneData)

//-----------------------------------------------------------------------------
// [SECTION] view
//-----------------------------------------------------------------------------

PL_BEGIN_STRUCT(plGpuViewData)

    int iClusterEnabled;
    int iClusterPointCount;
    int iClusterSpotCount;
    int iAOEnabled;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 0)~~~~~~~~~~~~~~~~

    vec4 afCascadeSplits;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 16)~~~~~~~~~~~~~~~~

    mat4 viewProjMat[4]; // main shadow camera (4 for cascades)
    // ~~~~~~~~~~~~~~~~256 bytes (offset 32)~~~~~~~~~~~~~~~~


    float fAspectRatio;
    int iContactShadowDepthIndex;
    int iTransmissionDepthIndex;
    float _fUnused2;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 288)~~~~~~~~~~~~~~~~
    
    int iTransmissionFrameBufferIndex;
    float fCameraRange;
    float fCameraNearZ;
    int   iCameraProjectType;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 304)~~~~~~~~~~~~~~~~

    vec4 tViewportSize;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 320)~~~~~~~~~~~~~~~~

    vec4 tCloudTransportCache; // xy: bindless resolved transport/depth (-1 unavailable), zw: reserved
    // ~~~~~~~~~~~~~~~~16 bytes (offset 336)~~~~~~~~~~~~~~~~

    vec4 tCameraPos; // viewing camera
    // ~~~~~~~~~~~~~~~~16 bytes (offset 352)~~~~~~~~~~~~~~~~

    mat4 tCameraView[6]; // viewing camera (6 for probes)
    // ~~~~~~~~~~~~~~~~384 bytes (offset 368)~~~~~~~~~~~~~~~~

    mat4 tCameraViewInv[6]; // viewing camera (6 for probes)
    // ~~~~~~~~~~~~~~~~384 bytes (offset 752)~~~~~~~~~~~~~~~~

    mat4 tCameraProjection[6]; // viewing camera (6 for probes)
    // ~~~~~~~~~~~~~~~~384 bytes (offset 1136)~~~~~~~~~~~~~~~~

    mat4 tCameraProjectionInv[6]; // viewing camera (6 for probes)
    // ~~~~~~~~~~~~~~~~384 bytes (offset 1520)~~~~~~~~~~~~~~~~

    mat4 tCameraViewProjection[6]; // viewing camera (6 for probes)
    // ~~~~~~~~~~~~~~~~384 bytes (offset 1904)~~~~~~~~~~~~~~~~

    mat4 tInvViewMatNoTranslation[6]; // viewing camera (6 for probes)
    // ~~~~~~~~~~~~~~~~384 bytes (offset 2288)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~2672 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuViewData)

//-----------------------------------------------------------------------------
// [SECTION] grid
//-----------------------------------------------------------------------------

PL_BEGIN_STRUCT(plGpuDynGrid)

    float fCameraXPos;
    float fCameraZPos;
    float fGridSize;
    float fGridCellSize;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 0)~~~~~~~~~~~~~~~~

    float fGridMinPixelsBetweenCells;
    int   _iUnused0;
    int   _iUnused1;
    int   _iUnused2;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 16)~~~~~~~~~~~~~~~~

    vec4 tViewDirection;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 32)~~~~~~~~~~~~~~~~

    vec4 tGridColorThin;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 48)~~~~~~~~~~~~~~~~

    vec4 tGridColorThick;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 64)~~~~~~~~~~~~~~~~

    mat4 tCameraViewProjection;
    // ~~~~~~~~~~~~~~~~64 bytes (offset 80)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~144 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuDynGrid)

//-----------------------------------------------------------------------------
// [SECTION] post processing
//-----------------------------------------------------------------------------

PL_BEGIN_STRUCT(plGpuDynFXAA)
    int iWidth;
    int iHeight;
    uint _auUnused0;
    uint _auUnused1;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 0)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~16 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuDynFXAA)

PL_BEGIN_STRUCT(plGpuDynAOData)
    mat4  tProjection;
    // ~~~~~~~~~~~~~~~~64 bytes (offset 0)~~~~~~~~~~~~~~~~
    mat4  tProjectionInv;
    // ~~~~~~~~~~~~~~~~64 bytes (offset 64)~~~~~~~~~~~~~~~~
    mat4  tView;
    // ~~~~~~~~~~~~~~~~64 bytes (offset 128)~~~~~~~~~~~~~~~~
    vec2  tViewportSize;
    int   iLevel;
    int   iMode;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 192)~~~~~~~~~~~~~~~~
    float fRadius;
    float fStrength;
    float fBias;
    int   iLevels;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 208)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~224 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuDynAOData)

PL_BEGIN_STRUCT(plGpuDynSSGI)
    mat4 tProjection;
    // ~~~~~~~~~~~~~~~~64 bytes (offset 0)~~~~~~~~~~~~~~~~
    mat4 tProjectionInv;
    // ~~~~~~~~~~~~~~~~64 bytes (offset 64)~~~~~~~~~~~~~~~~
    mat4 tView;
    // ~~~~~~~~~~~~~~~~64 bytes (offset 128)~~~~~~~~~~~~~~~~
    vec4 tParameters;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 192)~~~~~~~~~~~~~~~~
    uint uWidth;
    uint uHeight;
    uint uRayCount;
    uint uStepCount;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 208)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~224 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuDynSSGI)

PL_BEGIN_STRUCT(plGpuDynBloomData)
    float blurRadius;
    float bloomStrength;
    int isLowestMip;
    int iMipLevel;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 0)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~16 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuDynBloomData)

PL_BEGIN_STRUCT(plGpuDynTonemap)

    int   iMode;
    float fExposure;
    float fBrightness;
    float fContrast;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 0)~~~~~~~~~~~~~~~~

    float fSaturation;
    uint _auUnused0;
    uint _auUnused1;
    uint _auUnused2;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 16)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~32 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuDynTonemap)

//-----------------------------------------------------------------------------
// [SECTION] environment filtering
//-----------------------------------------------------------------------------

PL_BEGIN_STRUCT(plGpuDynFilterSpec)

    int   iResolution;
    float fRoughness;
    int   iSampleCount;
    int   iWidth;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 0)~~~~~~~~~~~~~~~~

    float fLodBias;
    int   iCurrentMipLevel;
    int   iFaceOffset;
    int   iFaceCount;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 16)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~32 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuDynFilterSpec)

//-----------------------------------------------------------------------------
// [SECTION] shadows
//-----------------------------------------------------------------------------

PL_BEGIN_STRUCT(plGpuDynShadow)

    int iIndex;
    int iDataOffset;
    int iVertexOffset;
    int iMaterialIndex;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 0)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~16 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuDynShadow)

//-----------------------------------------------------------------------------
// [SECTION] skinning
//-----------------------------------------------------------------------------

PL_BEGIN_STRUCT(plGpuDynSkinData)

    int  iSourceDataOffset;
    int  iDestDataOffset;
    int  iDestVertexOffset;
    uint uMaxSize;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 0)~~~~~~~~~~~~~~~~

    uint uMatrixOffset;
    int  _iUnused0;
    int  _iUnused1;
    int  _iUnused2;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 16)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~32 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuDynSkinData)

//-----------------------------------------------------------------------------
// [SECTION] skybox
//-----------------------------------------------------------------------------

PL_BEGIN_STRUCT(plGpuDynSkybox)

    uint uGlobalIndex;
    uint _auUnused0;
    uint _auUnused1;
    uint _auUnused2;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 0)~~~~~~~~~~~~~~~~

    mat4 tModel;
    // ~~~~~~~~~~~~~~~~64 bytes (offset 16)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~80 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuDynSkybox)

//-----------------------------------------------------------------------------
// [SECTION] outlines
//-----------------------------------------------------------------------------

PL_BEGIN_STRUCT(plGpuDynPost)

    float fTargetWidth;
    float fXScale;
    float fYScale;
    uint  _unused0;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 0)~~~~~~~~~~~~~~~~

    vec4 tOutlineColor;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 16)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~32 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuDynPost)

//-----------------------------------------------------------------------------
// [SECTION] picking
//-----------------------------------------------------------------------------

PL_BEGIN_STRUCT(plGpuDynPick)

    uint uID;
    uint _unused0;
    uint _unused1;
    uint _unused2;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 0)~~~~~~~~~~~~~~~~

    vec4 tMousePos;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 16)~~~~~~~~~~~~~~~~
    
    mat4 tModel;
    // ~~~~~~~~~~~~~~~~64 bytes (offset 32)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~96 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuDynPick)

//-----------------------------------------------------------------------------
// [SECTION] lights
//-----------------------------------------------------------------------------

PL_BEGIN_STRUCT(plGpuPointLight)

    vec3  tPosition;
    float fIntensity;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 0)~~~~~~~~~~~~~~~~

    vec3  tColor;
    float fRange;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 16)~~~~~~~~~~~~~~~~

    int iShadowIndex;
    int iContactShadow;
    int _unused1;
    int iCastShadow;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 32)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~48 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuPointLight)

PL_BEGIN_STRUCT(plGpuSpotLight)

    vec3  tPosition;
    float fIntensity;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 0)~~~~~~~~~~~~~~~~

    vec3  tDirection;
    float fInnerConeCos;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 16)~~~~~~~~~~~~~~~~

    vec3  tColor;
    float fRange;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 32)~~~~~~~~~~~~~~~~

    int iShadowIndex;
    int iCastShadow;
    float fOuterConeCos;
    int iContactShadow;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 48)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~64 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuSpotLight)

PL_BEGIN_STRUCT(plGpuDirectionLight)

    int iShadowIndex;
    int iContactShadow;
    int iCastShadow;
    float fIntensity;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 0)~~~~~~~~~~~~~~~~

    vec3 tDirection;
    int _unused1;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 16)~~~~~~~~~~~~~~~~

    vec3 tColor;
    int _unused2;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 32)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~48 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuDirectionLight)

PL_BEGIN_STRUCT(plGpuProbe)

    vec3  tPosition;
    float fRangeSqr;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 0)~~~~~~~~~~~~~~~~

    uint uLambertianEnvSampler;
    uint uGGXEnvSampler;
    int  uCharlieEnvSampler;
    int  iParallaxCorrection;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 16)~~~~~~~~~~~~~~~~

    vec4 tMin;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 32)~~~~~~~~~~~~~~~~
    vec4 tMax;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 48)~~~~~~~~~~~~~~~~

    int iMips;
    int _unused1;
    int _unused2;
    int _unused3;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 64)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~80 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuProbe)

PL_BEGIN_STRUCT(plGpuPointLightShadow)
    
    mat4 viewProjMat[6];
    // ~~~~~~~~~~~~~~~~384 bytes (offset 0)~~~~~~~~~~~~~~~~

    int iShadowMapTexIdx;
    float fFactor;
    float fXOffset;
    float fYOffset;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 384)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~400 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuPointLightShadow)

PL_BEGIN_STRUCT(plGpuSpotLightShadow)
    
    mat4 viewProjMat;
    // ~~~~~~~~~~~~~~~~64 bytes (offset 0)~~~~~~~~~~~~~~~~

    int iShadowMapTexIdx;
    float fFactor;
    float fXOffset;
    float fYOffset;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 64)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~80 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuSpotLightShadow)

PL_BEGIN_STRUCT(plGpuDirectionLightShadow)
    
    mat4 viewProjMat;
    // ~~~~~~~~~~~~~~~~64 bytes (offset 0)~~~~~~~~~~~~~~~~

    int iShadowMapTexIdx;
    float fFactor;
    float fXOffset;
    float fYOffset;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 64)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~80 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuDirectionLightShadow)

PL_BEGIN_STRUCT(plGpuDynDeferredLighting)
    
    uint uGlobalIndex;
    int  iLightIndex;
    int _iUnusedProbeCount;
    int iObjectShaderFlags;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 0)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~16 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuDynDeferredLighting)

PL_BEGIN_STRUCT(plShadowInstanceBufferData)

    uint uTransformIndex;
    int iViewportIndex;
    uint _uUnused0;
    uint _uUnused1;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 0)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~16 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plShadowInstanceBufferData)

//-----------------------------------------------------------------------------
// [SECTION] materials
//-----------------------------------------------------------------------------

PL_BEGIN_STRUCT(plGpuWater)
    vec4 tWaves; // mode, Gerstner steepness, FFT height bound, choppiness
    // ~~~~~~~~~~~~~~~~16 bytes (offset 0)~~~~~~~~~~~~~~~~
    vec4 tOcean; // texture index, patch length, resolution, unused
    // ~~~~~~~~~~~~~~~~16 bytes (offset 16)~~~~~~~~~~~~~~~~
    vec4 tSwell0; // swell0_amplitude, swell0_wavelength, swell0_speed, swell0_heading_degrees
    // ~~~~~~~~~~~~~~~~16 bytes (offset 32)~~~~~~~~~~~~~~~~
    vec4 tSwell1; // swell1_amplitude, swell1_wavelength, swell1_speed, swell1_heading_degrees
    // ~~~~~~~~~~~~~~~~16 bytes (offset 48)~~~~~~~~~~~~~~~~
    vec4 tDetail; // detail_scale, detail_speed, foam_speed, foam_scale
    // ~~~~~~~~~~~~~~~~16 bytes (offset 64)~~~~~~~~~~~~~~~~
    vec4 tShore; // shore_strength, shore_depth, shore_ray_fade_start, shore_ray_fade_end
    // ~~~~~~~~~~~~~~~~16 bytes (offset 80)~~~~~~~~~~~~~~~~
    vec4 tCaps; // whitecap_strength, crest_start, crest_full, slope_start
    // ~~~~~~~~~~~~~~~~16 bytes (offset 96)~~~~~~~~~~~~~~~~
    vec4 tAppearance; // slope_full, minimum_roughness, foam_roughness, foam_normal_flatten
    // ~~~~~~~~~~~~~~~~16 bytes (offset 112)~~~~~~~~~~~~~~~~
    vec4 tAbsorption; // RGB transmission at reference distance, reference distance
    // ~~~~~~~~~~~~~~~~16 bytes (offset 128)~~~~~~~~~~~~~~~~
    vec4 tScattering; // RGB scattering color, coefficient per world unit
    // ~~~~~~~~~~~~~~~~16 bytes (offset 144)~~~~~~~~~~~~~~~~
    vec4 tDistortion; // refraction strength, maximum offset in pixels, reserved, reserved
    // ~~~~~~~~~~~~~~~~16 bytes (offset 160)~~~~~~~~~~~~~~~~
    vec4 tOptics; // refraction_max_distance, scattering_direction, refraction_edge_fade_pixels, refraction_intersection_fade
    // ~~~~~~~~~~~~~~~~16 bytes (offset 176)~~~~~~~~~~~~~~~~
    vec4 tFoamColorDebug; // foam_red, foam_green, foam_blue, debug_view
    // ~~~~~~~~~~~~~~~~16 bytes (offset 192)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~208 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuWater)

PL_BEGIN_STRUCT(plGpuMaterial)

    float fMetallicFactor;
    float fRoughnessFactor;
    float fClearcoatFactor;
    float fClearcoatRoughnessFactor;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 0)~~~~~~~~~~~~~~~~

    vec4 tBaseColorFactor;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 16)~~~~~~~~~~~~~~~~

    vec3 tEmissiveFactor;
    float fEmissiveStrength;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 32)~~~~~~~~~~~~~~~~

    vec3 tSheenColorFactor;
    float fSheenRoughnessFactor;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 48)~~~~~~~~~~~~~~~~
    
    float fAlphaCutoff;
    float fOcclusionStrength;
    float fNormalMapStrength;
    float fThickness;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 64)~~~~~~~~~~~~~~~~

    vec3  tAttenuationColor;
    float fAttenuationDistance;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 80)~~~~~~~~~~~~~~~~

    float fIridescenceFactor;
    float fIridescenceIor;
    float fIridescenceThicknessMax;
    float fIridescenceThicknessMin;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 96)~~~~~~~~~~~~~~~~

    vec3 tAnisotropy;
    float fTransmissionFactor;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 112)~~~~~~~~~~~~~~~~

    float fDispersion;
    float fIor;
    int   eAlphaMode;
    int   iTextureMappingFlags;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 128)~~~~~~~~~~~~~~~~

    int iMaterialFlags;
    int _iUnused0;
    int _iUnused1;
    int _iUnused2;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 144)~~~~~~~~~~~~~~~~

    vec3 tDiffuseTransmissionColor;
    float fDiffuseTransmission;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 160)~~~~~~~~~~~~~~~~

    plGpuWater tWater;
    // ~~~~~~~~~~~~~~~~208 bytes (offset 176)~~~~~~~~~~~~~~~~

    int aiTextureUVSet[PL_TEXTURE_COUNT];
    // ~~~~~~~~~~~~~~~~80 bytes (offset 384)~~~~~~~~~~~~~~~~
    int aiTextureIndices[PL_TEXTURE_COUNT];
    // ~~~~~~~~~~~~~~~~80 bytes (offset 464)~~~~~~~~~~~~~~~~

    mat4 atTextureTransforms[PL_TEXTURE_COUNT];
    // ~~~~~~~~~~~~~~~~1280 bytes (offset 544)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~1824 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuMaterial)

PL_BEGIN_STRUCT(plCloudTemporalData)
    mat4 tPreviousViewProjection;
    // ~~~~~~~~~~~~~~~~64 bytes (offset 0)~~~~~~~~~~~~~~~~
    vec4 tPreviousCamera;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 64)~~~~~~~~~~~~~~~~
    vec4 tParams; // history weight (zero invalidates), delta time, reserved, reserved
    // ~~~~~~~~~~~~~~~~16 bytes (offset 80)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~96 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plCloudTemporalData)

#endif // PL_SHADER_INTEROP_RENDERER_H
