#ifndef PL_SHADER_INTEROP_RENDERER_H
#define PL_SHADER_INTEROP_RENDERER_H

#include "pl_shader_interop.h"

//-----------------------------------------------------------------------------
// [SECTION] common
//-----------------------------------------------------------------------------

PL_BEGIN_STRUCT(plGpuGlobalData)

    vec4 tViewportSize;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~

    vec4 tViewportInfo;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~

    vec4 tCameraPos;
    // ~~~~~~~~~~~~~~~~16 bytes~~~~~~~~~~~~~~~~

    mat4 tCameraView;
    // ~~~~~~~~~~~~~~~~64 bytes~~~~~~~~~~~~~~~~

    mat4 tCameraProjection;
    // ~~~~~~~~~~~~~~~~64 bytes~~~~~~~~~~~~~~~~

    mat4 tCameraViewProjection;
    // ~~~~~~~~~~~~~~~~64 bytes~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~240 bytes~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuGlobalData)

PL_BEGIN_STRUCT(plGpuDynData)

    int  iDataOffset;
    int  iVertexOffset;
    int  iMaterialIndex;
    uint uGlobalIndex;
PL_END_STRUCT(plGpuDynData)

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
// [SECTION] shadow
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
// [SECTION] post processing
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

PL_TYPEDEF(int, plLightType)
#define PL_LIGHT_TYPE_DIRECTIONAL 0
#define PL_LIGHT_TYPE_POINT 1
#define PL_LIGHT_TYPE_SPOT 2

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

//-----------------------------------------------------------------------------
// [SECTION] materials
//-----------------------------------------------------------------------------

PL_TYPEDEF(int, plMaterialInfoFlags)
#define PL_INFO_MATERIAL_METALLICROUGHNESS 1

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

PL_BEGIN_STRUCT(MaterialInfo)

    float perceptualRoughness;      // roughness value, as authored by the model creator (input to shader)
    vec3 f0;                        // full reflectance color (n incidence angle)

    float alphaRoughness;           // roughness mapped to a more linear change in the roughness (proposed by [2])
    vec3 c_diff;

    vec3 f90;                       // reflectance color at grazing angle
    float metallic;

    vec3 baseColor;

PL_END_STRUCT(MaterialInfo)

#endif // PL_SHADER_INTEROP_RENDERER_H
