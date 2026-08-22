/*
   pl_model_loader_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] forward declarations
// [SECTION] struct #0
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] structs #1
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*

    Implementation:
        The provided implementation of this extension depends on the following
        APIs being available:

        * plResourceI (v1.x)
        * plEcsI      (v1.x)
        * plFileI     (v1.x)
        * plVfsI      (v2.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_GLTF_EXT_H
#define PL_GLTF_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdint.h>
#include <stdbool.h>
#include "pl_math.h"
#include "pl_asset_ext.inl"

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plGltfI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plGltfImportOptions plGltfImportOptions;
typedef struct _plGltfImportResult  plGltfImportResult;
typedef struct _plModelLoaderData plModelLoaderData;
typedef union _plModelInstanceHandle plModelInstanceHandle;

// enums/flags
typedef int plGltfImportFlags;

// external 
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h
typedef union  _plEntity           plEntity;           // pl_ecs_ext.h


//-----------------------------------------------------------------------------
// [SECTION] struct #0
//-----------------------------------------------------------------------------

typedef union _plModelInstanceHandle
{
    struct
    {
        uint32_t uIndex;
        uint32_t uGeneration;
    };
    uint64_t uData;
} plModelInstanceHandle;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_gltf_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_gltf_ext(plApiRegistryI*, bool reload);

// import
PL_API bool pl_gltf_import(const char* path, const plGltfImportOptions*, plGltfImportResult*);

PL_API    plModelInstanceHandle pl_gltf_load       (plComponentLibrary*, const char* pcPath, const plMat4* ptTransform);
PL_API const plModelLoaderData* pl_gltf_get_objects(plModelInstanceHandle);
PL_API void                     pl_gltf_free_data  (plModelInstanceHandle);

// just use with GLTF models for now
PL_API bool pl_gltf_get_animation_by_name(plModelInstanceHandle, const char* name, plEntity* entityOut);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plGltfI
{
    bool (*import)(const char* path, const plGltfImportOptions*, plGltfImportResult*);

    plModelInstanceHandle    (*load)       (plComponentLibrary*, const char* pcPath, const plMat4* ptTransform);
    const plModelLoaderData* (*get_objects)(plModelInstanceHandle);
    void                     (*free_data)  (plModelInstanceHandle);

    // just use with GLTF models for now
    bool (*get_animation_by_name)(plModelInstanceHandle, const char* name, plEntity* entityOut);

} plGltfI;

//-----------------------------------------------------------------------------
// [SECTION] structs #1
//-----------------------------------------------------------------------------

typedef struct _plGltfImportOptions
{
    plGltfImportFlags eFlags;
} plGltfImportOptions;

typedef struct _plGltfImportResult
{
    plAssetHandle* atTextures;
    uint32_t       uTextureCount;

    plAssetHandle* atMaterials;
    uint32_t       uMaterialCount;

    plAssetHandle* atMeshes;
    uint32_t       uMeshCount;
    
    plAssetHandle* atAnimations;
    uint32_t       uAnimationCount;

    plAssetHandle* atSkeletons;
    uint32_t       uSkeletonCount;

    // optional imported scene
    // plEntity*      atRootEntities;
    // uint32_t       uRootEntityCount;

    // [INTERNAL]
    uint8_t* _puData;
    size_t   _szDataSize;
} plGltfImportResult;

typedef struct _plModelLoaderData
{
    uint32_t  uObjectCount;
    plEntity* atObjects;
} plModelLoaderData;

enum _plGltfImportFlags
{
    PL_GLTF_IMPORT_FLAGS_NONE                  = 0,
    PL_GLTF_IMPORT_FLAGS_IMPORT_MESHES         = 1 << 0,
    PL_GLTF_IMPORT_FLAGS_IMPORT_MATERIALS      = 1 << 1,
    PL_GLTF_IMPORT_FLAGS_IMPORT_TEXTURES       = 1 << 2,
    PL_GLTF_IMPORT_FLAGS_IMPORT_ANIMATIONS     = 1 << 3,
    PL_GLTF_IMPORT_FLAGS_IMPORT_SKELETONS      = 1 << 4,
    // PL_GLTF_IMPORT_FLAGS_IMPORT_SCENES         = 1 << 5,

    PL_GLTF_IMPORT_FLAGS_IMPORT_ALL_ASSETS     = PL_GLTF_IMPORT_FLAGS_IMPORT_MESHES | PL_GLTF_IMPORT_FLAGS_IMPORT_MATERIALS | PL_GLTF_IMPORT_FLAGS_IMPORT_TEXTURES | PL_GLTF_IMPORT_FLAGS_IMPORT_ANIMATIONS | PL_GLTF_IMPORT_FLAGS_IMPORT_SKELETONS,

    // misc
    // PL_GLTF_IMPORT_FLAGS_SPLIT_MESH_PRIMITIVES = 1 << 10,
    // PL_GLTF_IMPORT_FLAGS_GENERATE_TANGENTS     = 1 << 11,
};

#ifdef __cplusplus
}
#endif

#endif // PL_GLTF_EXT_H