/*
   pl_prototype.h

   * temporary locations for testing new APIs
   * things here will change wildly!
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

#ifndef PL_PROTOTYPE_H
#define PL_PROTOTYPE_H

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_MATERIAL_MAX_NAME_LENGTH
    #define PL_MATERIAL_MAX_NAME_LENGTH 1024
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>    // uint*_t
#include <stdbool.h>   // bool
#include "pl_math.h"
#include "pl_graphics_vulkan.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// forward declarations
typedef struct _plObjectInfo    plObjectInfo;
typedef struct _plMaterialInfo  plMaterialInfo;
typedef struct _plGlobalInfo    plGlobalInfo;
typedef struct _plMaterial      plMaterial;
typedef struct _plScene         plScene;
typedef struct _plNode          plNode;
typedef struct _plAssetRegistry plAssetRegistry;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// asset registry
void pl_setup_asset_registry  (plGraphics* ptGraphics, plAssetRegistry* ptRegistryOut);
void pl_cleanup_asset_registry(plAssetRegistry* ptRegistry);

// materials
void pl_initialize_material   (plMaterial* ptMaterial, const char* pcName);

// transform nodes
void pl_update_nodes          (plGraphics* ptGraphics, plNode* acNodes, uint32_t uCurrentNode, plMat4* ptMatrix);

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
    plBindGroup tMaterialBindGroup;
    uint64_t    ulShaderTextureFlags;
} plMaterial;

typedef struct _plNode
{
    char      acName[PL_MATERIAL_MAX_NAME_LENGTH];
    uint32_t  uMesh;
    uint32_t* sbuMeshes;
    plMat4    tFinalTransform;
    plMat4    tMatrix;
    plVec3    tTranslation;
    plVec3    tScale;
    plVec4    tRotation;
    uint32_t* sbuChildren;
} plNode;

typedef struct _plScene
{
    char      acName[PL_MATERIAL_MAX_NAME_LENGTH];
    uint32_t* sbuRootNodes;
} plScene;

typedef struct _plAssetRegistry
{
    plGraphics*     ptGraphics;
    plMaterial*     sbtMaterials;
    uint32_t        uDummyTexture;
} plAssetRegistry;

#endif // PL_PROTOTYPE_H