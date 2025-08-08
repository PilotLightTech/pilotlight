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
// [SECTION] tonemapping
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

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#define PL_MAX_BINDLESS_TEXTURES 4096
#define PL_MAX_BINDLESS_TEXTURE_SLOT 8
#define PL_MAX_BINDLESS_CUBE_TEXTURE_SLOT PL_MAX_BINDLESS_TEXTURES + PL_MAX_BINDLESS_TEXTURE_SLOT

#define PL_INFO_MATERIAL_METALLICROUGHNESS 1

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

PL_BEGIN_ENUM(plRenderingFlags)
    PL_ENUM_ITEM(PL_RENDERING_FLAG_USE_PUNCTUAL, 1 << 0)
    PL_ENUM_ITEM(PL_RENDERING_FLAG_USE_IBL, 1 << 1)
    PL_ENUM_ITEM(PL_RENDERING_FLAG_SHADOWS, 1 << 2)
PL_END_ENUM

PL_BEGIN_ENUM(plMeshFormatFlags)
    PL_ENUM_ITEM(PL_MESH_FORMAT_FLAG_NONE,                 0)
    PL_ENUM_ITEM(PL_MESH_FORMAT_FLAG_HAS_POSITION,   1 <<  0) 
    PL_ENUM_ITEM(PL_MESH_FORMAT_FLAG_HAS_NORMAL,     1 <<  1) 
    PL_ENUM_ITEM(PL_MESH_FORMAT_FLAG_HAS_TANGENT,    1 <<  2) 
    PL_ENUM_ITEM(PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0, 1 <<  3) 
    PL_ENUM_ITEM(PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1, 1 <<  4) 
    PL_ENUM_ITEM(PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_2, 1 <<  5) 
    PL_ENUM_ITEM(PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_3, 1 <<  6) 
    PL_ENUM_ITEM(PL_MESH_FORMAT_FLAG_HAS_COLOR_0,    1 <<  7) 
    PL_ENUM_ITEM(PL_MESH_FORMAT_FLAG_HAS_COLOR_1,    1 <<  8) 
    PL_ENUM_ITEM(PL_MESH_FORMAT_FLAG_HAS_JOINTS_0,   1 <<  9) 
    PL_ENUM_ITEM(PL_MESH_FORMAT_FLAG_HAS_JOINTS_1,   1 << 10)
    PL_ENUM_ITEM(PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0,  1 << 11)
    PL_ENUM_ITEM(PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1,  1 << 12)
PL_END_ENUM

PL_BEGIN_ENUM(plTonemapMode)
    PL_ENUM_ITEM(PL_TONEMAP_MODE_NONE, 0)
    PL_ENUM_ITEM(PL_TONEMAP_MODE_ACES, 1)
    PL_ENUM_ITEM(PL_TONEMAP_MODE_REINHARD, 2)
PL_END_ENUM

PL_BEGIN_ENUM(plLightType)
    PL_ENUM_ITEM(PL_LIGHT_TYPE_DIRECTIONAL, 0)
    PL_ENUM_ITEM(PL_LIGHT_TYPE_POINT, 1)
    PL_ENUM_ITEM(PL_LIGHT_TYPE_SPOT, 2)
PL_END_ENUM

PL_BEGIN_ENUM(plTextureMappingFlags)
    PL_ENUM_ITEM(PL_HAS_BASE_COLOR_MAP, 1 << 0)
    PL_ENUM_ITEM(PL_HAS_NORMAL_MAP, 1 << 1)
    PL_ENUM_ITEM(PL_HAS_EMISSIVE_MAP, 1 << 2)
    PL_ENUM_ITEM(PL_HAS_OCCLUSION_MAP, 1 << 3)
    PL_ENUM_ITEM(PL_HAS_METALLIC_ROUGHNESS_MAP, 1 << 4)
PL_END_ENUM

//-----------------------------------------------------------------------------
// [SECTION] common
//-----------------------------------------------------------------------------

PL_BEGIN_STRUCT(plGpuDynData)
    int  iDataOffset;
    int  iVertexOffset;
    int  iMaterialIndex;
    uint uGlobalIndex;
PL_END_STRUCT(plGpuDynData)

//-----------------------------------------------------------------------------
// [SECTION] scene
//-----------------------------------------------------------------------------

PL_BEGIN_STRUCT(plGpuSceneData)

    vec4 tUnused;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuSceneData)

//-----------------------------------------------------------------------------
// [SECTION] view
//-----------------------------------------------------------------------------

PL_BEGIN_STRUCT(plGpuViewData)

    vec4 tViewportSize;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~

    vec4 tCameraPos;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~

    mat4 tCameraView;
    // ~~~~~~~~~~~~~~~~64 bytes~~~~~~~~~~~~~~~~

    mat4 tCameraProjection;
    // ~~~~~~~~~~~~~~~~64 bytes~~~~~~~~~~~~~~~~

    mat4 tCameraViewProjection;
    // ~~~~~~~~~~~~~~~~64 bytes~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~224 bytes~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuViewData)

//-----------------------------------------------------------------------------
// [SECTION] tonemapping
//-----------------------------------------------------------------------------

PL_BEGIN_STRUCT(plGpuDynTonemap)

    int   iMode;
    float fExposure;
    float fBrightness;
    float fContrast;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~

    float fSaturation;
    uint _auUnused0;
    uint _auUnused1;
    uint _auUnused2;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~32 bytes~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuDynTonemap)

//-----------------------------------------------------------------------------
// [SECTION] environment filtering
//-----------------------------------------------------------------------------

PL_BEGIN_STRUCT(plGpuDynFilterSpec)

    int   iResolution;
    float fRoughness;
    int   iSampleCount;
    int   iWidth;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~

    float fLodBias;
    int   iDistribution;
    int   iIsGeneratingLut;
    int   iCurrentMipLevel;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~32 bytes~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuDynFilterSpec)

//-----------------------------------------------------------------------------
// [SECTION] shadows
//-----------------------------------------------------------------------------

PL_BEGIN_STRUCT(plGpuDynShadow)

    int iIndex;
    int iDataOffset;
    int iVertexOffset;
    int iMaterialIndex;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~

    mat4 tInverseWorld;
    // ~~~~~~~~~~~~~~~~64 bytes~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~80 bytes~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuDynShadow)

//-----------------------------------------------------------------------------
// [SECTION] skinning
//-----------------------------------------------------------------------------

PL_BEGIN_STRUCT(plGpuDynSkinData)

    int  iSourceDataOffset;
    int  iDestDataOffset;
    int  iDestVertexOffset;
    uint uMaxSize;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~

    mat4 tInverseWorld;
    // ~~~~~~~~~~~~~~~~64 bytes~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~80 bytes~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuDynSkinData)

//-----------------------------------------------------------------------------
// [SECTION] skybox
//-----------------------------------------------------------------------------

PL_BEGIN_STRUCT(plGpuDynSkybox)

    uint uGlobalIndex;
    uint _auUnused0;
    uint _auUnused1;
    uint _auUnused2;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~

    mat4 tModel;
    // ~~~~~~~~~~~~~~~~64 bytes~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~80 bytes~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuDynSkybox)

//-----------------------------------------------------------------------------
// [SECTION] outlines
//-----------------------------------------------------------------------------

PL_BEGIN_STRUCT(plGpuDynPost)

    float fTargetWidth;
    uint  _unused0;
    uint  _unused1;
    uint  _unused2;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~

    vec4 tOutlineColor;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~32 bytes~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuDynPost)

//-----------------------------------------------------------------------------
// [SECTION] picking
//-----------------------------------------------------------------------------

PL_BEGIN_STRUCT(plGpuDynPick)

    uint uID;
    uint _unused0;
    uint _unused1;
    uint _unused2;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~

    vec4 tMousePos;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~
    
    mat4 tModel;
    // ~~~~~~~~~~~~~~~~64 bytes~~~~~~~~~~~~~~~~
    
    // ~~~~~~~~~~~~~~~~96 bytes~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuDynPick)

//-----------------------------------------------------------------------------
// [SECTION] lights
//-----------------------------------------------------------------------------

PL_BEGIN_STRUCT(plGpuLight)

    vec3  tPosition;
    float fIntensity;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~

    vec3  tDirection;
    float fInnerConeCos;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~

    vec3  tColor;
    float fRange;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~

    int iShadowIndex;
    int iCascadeCount;
    int iCastShadow;
    float fOuterConeCos;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~

    int iType;
    int _unused0;
    int _unused1;
    int _unused2;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~80 bytes~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuLight)

PL_BEGIN_STRUCT(plGpuProbe)

    vec3  tPosition;
    float fRangeSqr;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~

    uint  uLambertianEnvSampler;
    uint  uGGXEnvSampler;
    uint  uGGXLUT;
    int   iParallaxCorrection;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~

    vec4 tMin;
    vec4 tMax;
    // ~~~~~~~~~~~~~~~~32 bytes~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~64 bytes~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuProbe)

PL_BEGIN_STRUCT(plGpuLightShadow)
    
    vec4 cascadeSplits;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~
	
    mat4 viewProjMat[6];
    // ~~~~~~~~~~~~~~~~384 bytes~~~~~~~~~~~~~~~~

    int iShadowMapTexIdx;
    float fFactor;
    float fXOffset;
    float fYOffset;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~
    
    // ~~~~~~~~~~~~~~~~416 bytes~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuLightShadow)

PL_BEGIN_STRUCT(plGpuDynDeferredLighting)
    
    uint uGlobalIndex;
    uint _uUnused0;
    uint _uUnused1;
    uint _uUnused2;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuDynDeferredLighting)

PL_BEGIN_STRUCT(plShadowInstanceBufferData)

    uint uTransformIndex;
    int iViewportIndex;
    uint _uUnused0;
    uint _uUnused1;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~
PL_END_STRUCT(plShadowInstanceBufferData)

//-----------------------------------------------------------------------------
// [SECTION] materials
//-----------------------------------------------------------------------------

PL_BEGIN_STRUCT(plGpuMaterial)

    float fMetallicFactor;
    float fRoughnessFactor;
    int _unused0;
    int _unused1;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~

    vec4 tBaseColorFactor;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~

    vec3 tEmissiveFactor;
    float fEmissiveStrength;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~
    
    float fAlphaCutoff;
    float fOcclusionStrength;
    int iBaseColorUVSet;
    int iNormalUVSet;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~

    int iEmissiveUVSet;
    int iOcclusionUVSet;
    int iMetallicRoughnessUVSet;
    int iBaseColorTexIdx;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~

    int iNormalTexIdx;
    int iEmissiveTexIdx;
    int iMetallicRoughnessTexIdx;
    int iOcclusionTexIdx;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~96 bytes~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuMaterial)

#endif // PL_SHADER_INTEROP_RENDERER_H
