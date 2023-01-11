/*
   pl_renderer.h, v0.1 (WIP)
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] defines
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_RENDERER_H
#define PL_RENDERER_H

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_DECLARE_STRUCT
    #define PL_DECLARE_STRUCT(name) typedef struct _ ## name  name
#endif

#ifndef PL_MATERIAL_MAX_NAME_LENGTH
    #define PL_MATERIAL_MAX_NAME_LENGTH 1024
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>    // uint*_t
#include <stdbool.h>   // bool
#include "pl_graphics_vulkan.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// forward declarations
PL_DECLARE_STRUCT(plObjectInfo);
PL_DECLARE_STRUCT(plMaterialInfo);
PL_DECLARE_STRUCT(plGlobalInfo);
PL_DECLARE_STRUCT(plMaterial);
PL_DECLARE_STRUCT(plAssetRegistry);
PL_DECLARE_STRUCT(plRenderer);

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// asset registry
void     pl_setup_asset_registry  (plGraphics* ptGraphics, plAssetRegistry* ptRegistryOut);
void     pl_cleanup_asset_registry(plAssetRegistry* ptRegistry);

// materials
void     pl_initialize_material   (plMaterial* ptMaterial, const char* pcName);
uint32_t pl_create_material       (plAssetRegistry* ptRegistry, plMaterial* ptMaterial);

// renderer
void     pl_setup_renderer        (plGraphics* ptGraphics, plAssetRegistry* ptRegistry, plRenderer* ptRendererOut);
void     pl_cleanup_renderer      (plRenderer* ptRenderer);
void     pl_renderer_begin_frame  (plRenderer* ptRenderer);
void     pl_renderer_submit_meshes(plRenderer* ptRenderer, plMesh* ptMeshes, uint32_t* puMaterials, plBindGroup* ptBindGroup, uint32_t uConstantBuffer, uint32_t uMeshCount);

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plGlobalInfo
{
    plVec4 tAmbientColor;
    plVec4 tCameraPos;
    plMat4 tCameraView;
    plMat4 tCameraViewProj;
    float  fTime;
    int    _unused[3];
} plGlobalInfo;

typedef struct _plMaterialInfo
{
    plVec4 tAlbedo;
} plMaterialInfo;

typedef struct _plObjectInfo
{
    plMat4   tModel;
    uint32_t uVertexOffset;
    int      _unused[3];
} plObjectInfo;

typedef struct _plMaterial
{
    char acName[PL_MATERIAL_MAX_NAME_LENGTH];
    
    // properties
    plVec4 tAlbedo;
    float  fAlphaCutoff;
    bool   bDoubleSided;

    // maps
    uint32_t uAlbedoMap;
    uint32_t uNormalMap;
    uint32_t uEmissiveMap;

    // misc
    uint32_t    uShader;
    uint32_t    uMaterialConstantBuffer;
    plBindGroup tMaterialBindGroup;
    uint64_t    ulShaderTextureFlags;

} plMaterial;

typedef struct _plAssetRegistry
{
    plGraphics*     ptGraphics;
    plMaterial*     sbtMaterials;
    uint32_t        uDummyTexture;
} plAssetRegistry;

typedef struct _plRenderer
{
    plGraphics*      ptGraphics;
    plAssetRegistry* ptAssetRegistry;
    float*           sbfStorageBuffer;
    plDraw*          sbtDraws;
    plDrawArea*      sbtDrawAreas;
    plBindGroup      tGlobalBindGroup;
    uint32_t         uGlobalStorageBuffer;
    uint32_t         uGlobalConstantBuffer;   
} plRenderer;

#endif // PL_RENDERER_H